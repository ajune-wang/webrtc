/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_STREAM_RESET_HANDLER_H_
#define NET_DCSCTP_SOCKET_STREAM_RESET_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/chunk/reconfig_chunk.h"
#include "net/dcsctp/packet/parameter/incoming_ssn_reset_request_parameter.h"
#include "net/dcsctp/packet/parameter/outgoing_ssn_reset_request_parameter.h"
#include "net/dcsctp/packet/parameter/reconfiguration_response_parameter.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/rx/data_tracker.h"
#include "net/dcsctp/rx/reassembly_queue.h"
#include "net/dcsctp/socket/context.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/retransmission_queue.h"

namespace dcsctp {

// StreamResetHandler handles sending outgoing stream reset requests (to close
// an SCTP stream, which translates to closing a data channel).
//
// It also handles incoming "outgoing stream reset requests", when the peer
// wants to close its data channel.
class StreamResetHandler {
 public:
  StreamResetHandler(absl::string_view log_prefix,
                     Context* context,
                     TimerManager* timer_manager,
                     DataTracker* data_tracker,
                     ReassemblyQueue* reassembly_queue,
                     RetransmissionQueue* retransmission_queue)
      : log_prefix_(std::string(log_prefix) + "reset: "),
        ctx_(*context),
        data_tracker_(*data_tracker),
        reassembly_queue_(*reassembly_queue),
        retransmission_queue_(*retransmission_queue),
        reconfig_timer_(timer_manager->CreateTimer(
            "re-config",
            [this]() { return OnReconfigTimerExpiry(); },
            TimerOptions(DurationMs(0)))),
        next_outgoing_req_seq_nbr_(ReconfigRequestSN(*ctx_.my_initial_tsn())),
        last_processed_req_seq_nbr_(
            ReconfigRequestSN(*ctx_.peer_initial_tsn() - 1)) {}

  // Processes a stream stream reconfiguration chunk and may either return
  // absl::nullopt (on protocol errors), or a list of responses - either 0, 1
  // or 2.
  absl::optional<std::vector<ReconfigurationResponseParameter>> Process(
      const ReConfigChunk& chunk);

  // Initiates reset of the provided streams. May be called multiple times, even
  // when an outgoing request is in progress.
  void ResetStreams(rtc::ArrayView<const StreamID> outgoing_streams);

  // Creates a Reset Streams request that must be sent if returned. Will start
  // the reconfig timer.
  absl::optional<ReConfigChunk> MakeStreamResetRequest();

  // A request (setting `current_request`) must have been created prior.
  ReConfigChunk MakeReconfigChunk();

  // Called when handling and incoming RE-CONFIG chunk.
  void HandleReConfig(ReConfigChunk chunk);

 private:
  struct CurrentRequest {
    CurrentRequest(TSN sender_last_assigned_tsn, std::vector<StreamID> streams)
        : req_seq_nbr(absl::nullopt),
          sender_last_assigned_tsn(sender_last_assigned_tsn),
          streams(std::move(streams)) {}
    // If this is set, this request has been sent. If it's not set, the request
    // has been prepared, but has not yet been sent. This is typically used when
    // the peer responded "in progress" and the same request (but a different
    // request number) must be sent again.
    absl::optional<ReconfigRequestSN> req_seq_nbr;
    TSN sender_last_assigned_tsn;
    // The streams that are to be reset in this request.
    std::vector<StreamID> streams;
  };

  // Called to validate an incoming RE-CONFIG chunk.
  bool Validate(const ReConfigChunk& chunk);

  // Called to validate the `req_seq_nbr`, that it's the next in sequence. If it
  // fails to validate, and returns false, it will also add a response to
  // `responses`.
  bool ValidateReqSeqNbr(
      ReconfigRequestSN req_seq_nbr,
      std::vector<ReconfigurationResponseParameter>& responses);

  // Called when this socket receives an outgoing stream reset request. It might
  // either be performed straight away, or have to be deferred, and the result
  // of that will be put in `responses`.
  void HandleResetOutgoing(
      const ParameterDescriptor& descriptor,
      std::vector<ReconfigurationResponseParameter>& responses);

  // Called when this socket receives an incoming stream reset request. This
  // isn't really supported, but a successful response is put in `responses`.
  void HandleResetIncoming(
      const ParameterDescriptor& descriptor,
      std::vector<ReconfigurationResponseParameter>& responses);

  // Called when receiving a response to an outgoing stream reset request. It
  // will either commit the stream resetting, if the operation was successful,
  // or will schedule a retry if it was deferred. And if it failed, the
  // operation will be rolled back.
  void HandleResponse(const ParameterDescriptor& descriptor);

  // Expiration handler for the Reconfig timer.
  absl::optional<DurationMs> OnReconfigTimerExpiry();

  const std::string log_prefix_;
  Context& ctx_;
  DataTracker& data_tracker_;
  ReassemblyQueue& reassembly_queue_;
  RetransmissionQueue& retransmission_queue_;
  const std::unique_ptr<Timer> reconfig_timer_;

  // Outgoing streams that have been requested to be reset, but hasn't yet
  // been included in an outgoing request.
  std::unordered_set<StreamID, StreamID::Hasher> streams_to_reset_;
  ReconfigRequestSN next_outgoing_req_seq_nbr_;
  // Set when a request has been prepared.
  absl::optional<CurrentRequest> current_request_;

  // For incoming requests - last processed request sequence number.
  ReconfigRequestSN last_processed_req_seq_nbr_;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_SOCKET_STREAM_RESET_HANDLER_H_
