/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_RETRANSMISSION_QUEUE_H_
#define NET_DCSCTP_TX_RETRANSMISSION_QUEUE_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/retransmission_timeout.h"
#include "net/dcsctp/tx/send_queue.h"

namespace dcsctp {

// The RetransmissionQueue manage all DATA/I-DATA chunks that are in-flight and
// schedule them to be retransmitted if necessary.
//
// As congestion control is tightly connected with the state of transmitted
// packets, that's also managed here.
class RetransmissionQueue {
 public:
  static constexpr size_t kMinimumFragmentedPayload = 10;
  enum class State {
    // The chunk has been sent but not received yet (from the sender's point of
    // view, as no SACK has been received yet that reference this chunk).
    kInFlight,
    // A SACK has been received which explicitly marked this chunk as missing -
    // it's now NACKED and may be retransmitted.
    kNacked,
    // A chunk that will be retransmitted when possible.
    kToBeRetransmitted,
    // A SACK has been received which explicitly marked this chunk as received.
    kAcked,
    // A chunk whose message has expired or has been retransmitted too many
    // times (RFC3758).
    kAbandoned,
  };

  RetransmissionQueue(absl::string_view log_prefix,
                      TSN initial_tsn,
                      size_t a_rwnd,
                      SendQueue* producer,
                      std::function<void(DurationMs rtt)> on_new_rtt,
                      std::function<void()> on_chunk_producer_empty,
                      std::function<void()> on_clear_retransmission_counter,
                      Timer* t3_rtx,
                      const DcSctpOptions& options,
                      bool supports_partial_reliability = true,
                      bool use_message_interleaving = false);

  // Indicates if the provided SACK is valid. If it returns false, the SACK is
  // most likely a duplicate of something already seen.
  bool IsAcknowledgeValid(const SackChunk& sack) const;

  // Handles a received SACK.
  void HandleAcknowledge(TimeMs now, const SackChunk& sack);

  // Handles an expired retransmission timer.
  void HandleT3RtxTimerExpiry();

  // Returns a list of chunks to send that would fit in one SCTP packet with
  // `bytes_remaining_in_packet` bytes available. This may be further limited by
  // the congestion control windows. Note that `ShouldSendForwardTSN` must be
  // called prior to this method, to abandon expired chunks, as this method will
  // not expire any chunks.
  std::vector<std::pair<TSN, Data>> GetChunksToSend(
      TimeMs now,
      size_t bytes_remaining_in_packet);

  // Returns the internal state of all queued chunks. This is only used in
  // unit-tests.
  std::vector<std::pair<TSN, State>> GetChunkStates() const;

  // Returns the next TSN that will be allocated for sent DATA chunks.
  TSN next_tsn() const { return next_tsn_.Wrap(); }

  // Returns the size of the congestion window, in bytes. This is the number of
  // bytes that may be in-flight.
  size_t cwnd() const { return cwnd_; }

  // Overrides the current congestion window size.
  void set_cwnd(size_t cwnd) { cwnd_ = cwnd; }

  // Returns the current receiver window size.
  size_t rwnd() const { return rwnd_; }

  // Returns the number of bytes of packets that are in-flight.
  size_t outstanding_bytes() const { return outstanding_bytes_; }

  // Returns true if a FORWARD-TSN should be sent.
  bool ShouldSendForwardTsn(TimeMs now);
  // Creates a FORWARD-TSN chunk.
  ForwardTsnChunk CreateForwardTsn() const;
  // Creates an I-FORWARD-TSN chunk.
  IForwardTsnChunk CreateIForwardTsn() const;

  // See the SendQueue for a longer description of these methods related
  // to stream resetting.
  void PrepareResetStreams(rtc::ArrayView<const StreamID> streams);
  bool CanResetStreams() const;
  void CommitResetStreams();
  void RollbackResetStreams();

 private:
  enum class CongestionAlgorithmPhase {
    kSlowStart,
    kCongestionAvoidance,
  };

  // A fragmented message's DATA chunk while in the retransmission queue, and
  // its associated metadata.
  struct TxData {
    explicit TxData(Data data,
                    absl::optional<size_t> max_retransmissions,
                    TimeMs time_sent,
                    absl::optional<TimeMs> expires_at)
        : state(State::kInFlight),
          nack_count(0),
          num_retransmissions(0),
          max_retransmissions(max_retransmissions),
          time_sent(time_sent),
          expires_at(expires_at),
          data(std::move(data)) {}
    State state;
    size_t nack_count;
    size_t num_retransmissions;
    absl::optional<size_t> max_retransmissions;
    TimeMs time_sent;
    absl::optional<TimeMs> expires_at;
    Data data;
  };

  struct AckGapBlockInfo {
    explicit AckGapBlockInfo(UnwrappedTSN htna)
        : bytes_acked_by_new_gap_ack_blocks(0), htna(htna) {}
    // Bytes acked by gap blocks in this SACK.
    size_t bytes_acked_by_new_gap_ack_blocks;
    // Highest TSN Newly Acknowledged, an SCTP variable.
    UnwrappedTSN htna;
  };

  // Indicates if the congestion control algorithm is in "fast recovery".
  bool is_in_fast_recovery() const {
    return fast_recovery_exit_tsn_.has_value();
  }

  // Indicates if the congestion control algorithm is in "fast retransmit".
  bool is_in_fast_retransmit() const { return is_in_fast_retransmit_; }

  // Given a `cumulative_tsn_ack` from an incoming SACK, will remove those items
  // in the retransmission queue up until this value and will return the number
  // of bytes acked, as well as putting the TSN values of those chunks in
  // `acked_tsns`.
  size_t RemoveAcked(UnwrappedTSN cumulative_tsn_ack,
                     std::vector<TSN>& acked_tsns);

  // Will mark the chunks covered by the `gap_ack_blocks` from an incoming SACK
  // as "acked".
  AckGapBlockInfo AckGapBlocks(
      UnwrappedTSN cumulative_tsn_ack,
      rtc::ArrayView<const SackChunk::GapAckBlock> gap_ack_blocks,
      std::vector<TSN>& acked_tsns);

  // Mark chunks reported as "missing", as "nacked" or "to be retransmitted"
  // depending how many times this has happened. Only packets up until
  // htna (highest TSN newly acknowledged) are nacked/retransmitted. The method
  // will indicate if any (new) packet loss was detected, as return value.
  bool NackBetweenAckBlocks(
      UnwrappedTSN cumulative_tsn_ack,
      UnwrappedTSN htna,
      rtc::ArrayView<const SackChunk::GapAckBlock> gap_ack_blocks);

  // When a SACK chunk is received, this method will be called which _may_ call
  // into the `RetransmissionTimeout` to update the RTO.
  void UpdateRTT(TimeMs now, UnwrappedTSN cumulative_tsn_ack);

  // If the congestion control is in "fast recovery mode", this may be exited
  // now.
  void MaybeExitFastRecovery(UnwrappedTSN cumulative_tsn_ack);

  // If chunks have been ACKed, stop the retransmission timer.
  void StopT3RtxTimerOnIncreasedCumulativeTsnAck(
      UnwrappedTSN cumulative_tsn_ack);

  // Update the congestion control algorithm given as the cumulative ack TSN
  // value has increased, as reported in an incoming SACK chunk.
  void HandleIncreasedCumulativeTsnAck(size_t outstanding_bytes,
                                       size_t total_bytes_acked);
  // Update the congestion control algorithm, given as packet loss has been
  // detected, as reported in an incoming SACK chunk.
  void HandlePacketLoss(UnwrappedTSN htna);
  // Recalculate the number of in-flight payload bytes.
  void RecalculateOutstandingBytes();
  // Update the view of the receiver window size.
  void UpdateReceiverWindow(uint32_t a_rwnd);
  // Given `max_size` of space left in a packet, which chunks can be added to
  // it?
  std::vector<std::pair<TSN, Data>> GetChunksToBeRetransmitted(size_t max_size);
  // If there is data sent and not ACKED, ensure that the retransmission timer
  // is running.
  void StartT3RtxTimerIfOutstandingData();

  // Given the current time `now_ms`, expire chunks that have a limited
  // lifetime.
  void ExpireChunks(TimeMs now);
  // Given that a message fragment, `item` has expired, expire all other
  // fragments that share the same message - even never-before-sent fragments
  // that are still in the SendQueue.
  void ExpireAllFor(const RetransmissionQueue::TxData& item);

  // Returns the current congestion control algorithm phase.
  CongestionAlgorithmPhase phase() const {
    return (cwnd_ <= ssthresh_)
               ? CongestionAlgorithmPhase::kSlowStart
               : CongestionAlgorithmPhase::kCongestionAvoidance;
  }

  const DcSctpOptions options_;
  const bool partial_reliability_;
  const std::string log_prefix_;
  // The size of the data chunk (DATA/I-DATA) that is used.
  const size_t data_chunk_header_size_;
  const std::function<void(DurationMs rtt)> on_new_rtt_;
  const std::function<void()> on_chunk_producer_empty_;
  const std::function<void()> on_clear_retransmission_counter_;
  Timer& t3_rtx_;
  UnwrappedTSN::Unwrapper tsn_unwrapper_;

  // Congestion Window. Number of bytes that may be in-flight (sent, not acked).
  size_t cwnd_;
  // Receive Window. Number of bytes available in the receiver's RX buffer.
  size_t rwnd_;
  // Slow Start Threshold. See RFC4960.
  size_t ssthresh_;
  // Partial Bytes Acked. See RFC4960.
  size_t partial_bytes_acked_ = 0;
  // If set, fast recovery is enabled until this TSN has been cumulative
  // acked.
  absl::optional<UnwrappedTSN> fast_recovery_exit_tsn_ = absl::nullopt;
  // Indicates if the congestion algorithm is in fast retransmit.
  bool is_in_fast_retransmit_ = false;

  // Next TSN to used.
  UnwrappedTSN next_tsn_;
  // The last cumulative TSN ack number
  UnwrappedTSN last_cumulative_tsn_ack_;
  SendQueue& producer_;
  // All the outstanding data chunks that are in-flight and that have not been
  // cumulative acked. Note that it also contains chunks that have been acked in
  // gap ack blocks.
  std::map<UnwrappedTSN, TxData> outstanding_data_;
  // The number of bytes that are in-flight (sent and not acked).
  size_t outstanding_bytes_ = 0;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_TX_RETRANSMISSION_QUEUE_H_
