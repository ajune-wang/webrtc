/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_STREAM_SCHEDULER_H_
#define NET_DCSCTP_TX_STREAM_SCHEDULER_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/idata_chunk.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/tx/send_queue.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/strong_alias.h"

namespace dcsctp {

// A parameterized stream scheduler. Currently, it implements the round robin
// scheduling algorithm using virtual finish time. It is to be used as a part of
// a send queue and will track all active streams (streams that have any data
// that can be sent).
class StreamScheduler {
 private:
  class VirtualTime : public webrtc::StrongAlias<class VirtualTimeTag, double> {
   public:
    constexpr explicit VirtualTime(const UnderlyingType& v)
        : webrtc::StrongAlias<class VirtualTimeTag, double>(v) {}

    static constexpr VirtualTime Zero() { return VirtualTime(0); }
  };
  using InverseWeight = webrtc::StrongAlias<class InverseWeightTag, double>;

 public:
  class StreamCallback {
   public:
    virtual ~StreamCallback() = default;

    // Produces a fragment of data to send. The current wall time is specified
    // as `now` and should be used to skip chunks with expired limited lifetime.
    // The parameter `max_size` specifies the maximum amount of actual payload
    // that may be returned. If these constraints prevents the stream from
    // sending some data, `absl::nullopt` should be returned.
    virtual absl::optional<SendQueue::DataToSend> Produce(TimeMs now,
                                                          size_t max_size) = 0;

    // Returns the number of payload bytes that is scheduled to be sent in the
    // next enqueued message, or zero if there are no enqueued messages or if
    // the stream has been actively paused.
    virtual size_t bytes_to_send_in_next_message() const = 0;
  };

  class Stream {
   public:
    StreamID stream_id() const { return stream_id_; }

    StreamPriority priority() const { return priority_; }
    void set_priority(StreamPriority priority);

    // Will activate the stream _if_ it has any data to send. That is, if the
    // callback to `bytes_to_send_in_next_message` returns non-zero. If the
    // callback returns zero, the stream will not be made active.
    void MaybeMakeActive();

    // Will remove the stream from the list of active streams, and will not try
    // to produce data from it. To make it active again, call `MaybeMakeActive`.
    void MakeInactive();

    // Make the scheduler move to another message, or another stream. This is
    // used to abort the scheduler from continuing producing fragments for the
    // current message in case it's deleted.
    void ForceReschedule() { parent_.ForceReschedule(); }

   private:
    friend class StreamScheduler;

    Stream(StreamScheduler* parent,
           StreamCallback* callback,
           StreamID stream_id,
           StreamPriority priority)
        : parent_(*parent),
          callback_(*callback),
          stream_id_(stream_id),
          priority_(priority),
          inverse_weight_(1.0 / priority.value()) {}

    // Produces a message from this stream. This will only be called on streams
    // that have data.
    absl::optional<SendQueue::DataToSend> Produce(TimeMs now, size_t max_size);

    void MakeActive(size_t bytes_to_send_next);
    void ForceMarkInactive();

    VirtualTime current_time() const { return current_virtual_time_; }
    VirtualTime next_finish_time() const { return next_finish_time_; }
    size_t bytes_to_send_in_next_message() const {
      return callback_.bytes_to_send_in_next_message();
    }

    VirtualTime CalculateFinishTime(size_t bytes_to_send_next) const;

    StreamScheduler& parent_;
    StreamCallback& callback_;
    const StreamID stream_id_;
    StreamPriority priority_;
    InverseWeight inverse_weight_;
    // This outgoing stream's "current" virtual_time.
    VirtualTime current_virtual_time_ = VirtualTime::Zero();
    VirtualTime next_finish_time_ = VirtualTime::Zero();
  };

  explicit StreamScheduler(size_t mtu)
      : max_payload_bytes_(mtu - SctpPacket::kHeaderSize -
                           IDataChunk::kHeaderSize) {}

  std::unique_ptr<Stream> CreateStream(StreamCallback* callback,
                                       StreamID stream_id,
                                       StreamPriority priority) {
    return absl::WrapUnique(new Stream(this, callback, stream_id, priority));
  }

  void EnableMessageInterleaving(bool enabled) {
    enable_message_interleaving_ = enabled;
  }

  // Makes the scheduler stop producing message from the current stream and
  // re-evaluates which stream to produce from.
  void ForceReschedule() { previous_message_has_ended_ = true; }

  // Produces a fragment of data to send. The current wall time is specified as
  // `now` and will be used to skip chunks with expired limited lifetime. The
  // parameter `max_size` specifies the maximum amount of actual payload that
  // may be returned. If no data can be produced, `absl::nullopt` is returned.
  absl::optional<SendQueue::DataToSend> Produce(TimeMs now, size_t max_size);

  rtc::ArrayView<Stream* const> ActiveStreamsForTesting() const {
    return rtc::MakeArrayView(&*active_streams_.cbegin(),
                              active_streams_.size());
  }

 private:
  struct ActiveStreamComparator {
    bool operator()(Stream* a, Stream* b) const {
      VirtualTime a_vft = a->next_finish_time();
      VirtualTime b_vft = b->next_finish_time();
      if (a_vft == b_vft) {
        return a->stream_id() < b->stream_id();
      }
      return a_vft < b_vft;
    }
  };

  bool IsConsistent() const;

  const size_t max_payload_bytes_;

  // The current virtual time, as defined in the WFQ algorithm.
  VirtualTime virtual_time_ = VirtualTime::Zero();

  // The current stream to send chunks from.
  Stream* current_stream_ = nullptr;

  bool enable_message_interleaving_ = false;

  // Indicates if the previous fragment sent was the end of a message. For
  // non-interleaved sending, this means that the next message may come from a
  // different stream. If not true, the next fragment must be produced from the
  // same stream as last time.
  bool previous_message_has_ended_ = true;

  // The currently active streams, ordered by virtual finish time.
  webrtc::flat_set<Stream*, ActiveStreamComparator> active_streams_;
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_TX_STREAM_SCHEDULER_H_
