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
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/tx/send_queue.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/strong_alias.h"

namespace dcsctp {

class StreamScheduler {
 private:
  class VirtualTime : public webrtc::StrongAlias<class VirtualTimeTag, double> {
   public:
    constexpr explicit VirtualTime(const UnderlyingType& v)
        : webrtc::StrongAlias<class VirtualTimeTag, double>(v) {}

    static constexpr VirtualTime Zero() { return VirtualTime(0); }
  };

 public:
  class StreamCallback {
   public:
    virtual ~StreamCallback() = default;
    virtual absl::optional<SendQueue::DataToSend> Produce(TimeMs, size_t) = 0;
    virtual size_t bytes_to_send_in_next_message() const = 0;
  };

  class Stream {
   public:
    Stream(StreamScheduler* parent,
           StreamCallback* callback,
           StreamID stream_id,
           StreamPriority priority)
        : parent_(*parent),
          callback_(*callback),
          stream_id_(stream_id),
          priority_(priority) {}

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
    void ForceReschedule() { parent_.ForceReschedule(); }

   private:
    friend class StreamScheduler;
    // Produces a message from this stream. This will only be called on streams
    // that have data.
    absl::optional<SendQueue::DataToSend> Produce(TimeMs now, size_t max_size);
    void MakeActive();

    void ForceMarkInactive();

    VirtualTime current_time() const { return current_virtual_time_; }
    VirtualTime next_finish_time() const { return next_finish_time_; }
    size_t bytes_to_send_in_next_message() const {
      return callback_.bytes_to_send_in_next_message();
    }

    // Returns the next virtual finish time for this stream.
    VirtualTime GetNextFinishTime() const;

    StreamScheduler& parent_;
    StreamCallback& callback_;
    const StreamID stream_id_;
    StreamPriority priority_;
    // This outgoing stream's "current" virtual_time.
    VirtualTime current_virtual_time_ = VirtualTime::Zero();
    VirtualTime next_finish_time_ = VirtualTime::Zero();
  };

  std::unique_ptr<Stream> CreateStream(StreamCallback* callback,
                                       StreamID stream_id,
                                       StreamPriority priority) {
    return std::make_unique<Stream>(this, callback, stream_id, priority);
  }

  void ForceReschedule() { previous_message_has_ended_ = true; }

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

  // The current virtual time, as defined in the WFQ algorithm.
  VirtualTime virtual_time_ = VirtualTime::Zero();

  // The current stream to send chunks from.
  Stream* current_stream_ = nullptr;

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
