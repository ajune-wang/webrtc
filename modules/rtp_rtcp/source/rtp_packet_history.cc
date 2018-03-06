/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packet_history.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
// Min packet size for BestFittingPacket() to honor.
constexpr size_t kMinPacketRequestBytes = 50;

// Utility function to get the absolute difference in size between the provided
// target size and the size of packet.
size_t SizeDiff(const std::unique_ptr<RtpPacketToSend>& packet, size_t size) {
  size_t packet_size = packet->size();
  if (packet_size > size) {
    return packet_size - size;
  }
  return size - packet_size;
}

}  // namespace

constexpr uint16_t RtpPacketHistory::kMaxCapacity;
constexpr int64_t RtpPacketHistory::kMinPacketDurationMs;
constexpr int RtpPacketHistory::kMinPacketDurationRtt;
constexpr int RtpPacketHistory::kPacketCullingDelayFactor;

RtpPacketHistory::PacketState::PacketState() = default;
RtpPacketHistory::PacketState::~PacketState() = default;
RtpPacketHistory::PacketState::PacketState(const PacketState&) = default;

RtpPacketHistory::StoredPacket::StoredPacket(
    std::unique_ptr<RtpPacketToSend> packet)
    : packet(std::move(packet)) {}
RtpPacketHistory::StoredPacket::StoredPacket(StoredPacket&&) = default;
RtpPacketHistory::StoredPacket& RtpPacketHistory::StoredPacket::operator=(
    RtpPacketHistory::StoredPacket&&) = default;
RtpPacketHistory::StoredPacket::~StoredPacket() = default;
RtpPacketHistory::PacketState RtpPacketHistory::StoredPacket::AsPacketState() {
  RtpPacketHistory::PacketState state;
  state.rtp_sequence_number = packet->SequenceNumber();
  state.transport_sequence_number = transport_sequence_number;
  state.send_time_ms = send_time_ms;
  state.capture_time_ms = packet->capture_time_ms();
  state.ssrc = packet->Ssrc();
  state.payload_size = packet->size();
  state.times_retransmitted = times_retransmitted;
  return state;
}

RtpPacketHistory::RtpPacketHistory(Clock* clock)
    : clock_(clock),
      number_to_store_(0),
      mode_(StorageMode::kDisabled),
      rtt_ms_(-1) {}

RtpPacketHistory::~RtpPacketHistory() {}

void RtpPacketHistory::SetStorePacketsStatus(StorageMode mode,
                                             uint16_t number_to_store) {
  rtc::CritScope cs(&lock_);
  if (mode != StorageMode::kDisabled && mode_ != StorageMode::kDisabled) {
    RTC_LOG(LS_WARNING) << "Purging packet history in order to re-set status.";
  }
  Reset();
  mode_ = mode;
  number_to_store_ = std::min(kMaxCapacity, number_to_store);
}

RtpPacketHistory::StorageMode RtpPacketHistory::GetStorageMode() const {
  rtc::CritScope cs(&lock_);
  return mode_;
}

void RtpPacketHistory::SetRtt(int64_t rtt_ms) {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK_GE(rtt_ms, 0);
  rtt_ms_ = rtt_ms;
}

void RtpPacketHistory::PutRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                                    StorageType type,
                                    rtc::Optional<int64_t> send_time) {
  RTC_DCHECK(packet);
  int64_t now_ms = clock_->TimeInMilliseconds();
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return;
  }

  CullOldPackets(now_ms);

  // Store packet.
  const uint16_t rtp_seq_no = packet->SequenceNumber();
  auto packet_it =
      packet_history_.emplace(packet->SequenceNumber(), std::move(packet));
  RTC_DCHECK(packet_it.second);
  StoredPacket& stored_packet = packet_it.first->second;

  if (stored_packet.packet->capture_time_ms() <= 0) {
    stored_packet.packet->set_capture_time_ms(now_ms);
  }
  stored_packet.send_time_ms = send_time;
  stored_packet.storage_type = type;
  stored_packet.times_retransmitted = 0;

  if (!start_seqno_) {
    start_seqno_.emplace(rtp_seq_no);
  }
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPacketAndSetSendTime(
    uint16_t sequence_number,
    bool verify_rtt) {
  rtc::CritScope cs(&lock_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  rtc::Optional<StoredPacketItr> packet_it =
      GetPacket(sequence_number, verify_rtt, now_ms);
  if (!packet_it) {
    return std::unique_ptr<RtpPacketToSend>();
  }

  RtpPacketHistory::StoredPacketItr packet = *packet_it;

  if (packet->second.send_time_ms) {
    ++packet->second.times_retransmitted;
  }

  // Update send-time and return copy of packet instance.
  packet->second.send_time_ms.emplace(now_ms);

  if (packet->second.storage_type == StorageType::kDontRetransmit) {
    // Non retransmittable packet, so call must come from paced sender.
    // Remove from history and return actual packet instance.
    return RemovePacket(packet);
  }
  return rtc::MakeUnique<RtpPacketToSend>(*packet->second.packet);
}

rtc::Optional<RtpPacketHistory::PacketState> RtpPacketHistory::GetPacketState(
    uint16_t sequence_number,
    bool verify_rtt) {
  rtc::CritScope cs(&lock_);
  rtc::Optional<StoredPacketItr> packet_it =
      GetPacket(sequence_number, verify_rtt, clock_->TimeInMilliseconds());
  if (!packet_it) {
    return rtc::nullopt;
  }

  return (*packet_it)->second.AsPacketState();
}

rtc::Optional<RtpPacketHistory::StoredPacketItr> RtpPacketHistory::GetPacket(
    uint16_t sequence_number,
    bool verify_rtt,
    int64_t now_ms) {
  if (mode_ == StorageMode::kDisabled) {
    return rtc::nullopt;
  }

  StoredPacketItr rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    // Packet not found.
    return rtc::nullopt;
  }

  const StoredPacket& stored_packet = rtp_it->second;
  if (stored_packet.send_time_ms) {
    // Send-time already set, this must be a retransmission.
    if (verify_rtt && stored_packet.times_retransmitted > 0 &&
        now_ms < *stored_packet.send_time_ms + rtt_ms_) {
      // This packet has already been retransmitted once, and the time since
      // that even is lower than on RTT. Ignore request as this packet is
      // likely already in the network pipe.
      return rtc::nullopt;
    }
  }

  return rtp_it;
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetBestFittingPacket(
    size_t packet_length) const {
  // TODO(sprang): Make this smarter, taking retransmit count etc into account.
  rtc::CritScope cs(&lock_);
  if (packet_length < kMinPacketRequestBytes || packet_history_.empty()) {
    return std::unique_ptr<RtpPacketToSend>();
  }

  rtc::Optional<size_t> min_diff;
  RtpPacketToSend* best_packet = nullptr;
  for (auto& it : packet_history_) {
    size_t diff = SizeDiff(it.second.packet, packet_length);
    if (!min_diff || diff < *min_diff) {
      min_diff.emplace(diff);
      best_packet = it.second.packet.get();
      if (diff == 0) {
        break;
      }
    }
  }

  return rtc::MakeUnique<RtpPacketToSend>(*best_packet);
}

void RtpPacketHistory::OnTransportSequenceCreated(
    uint16_t rtp_sequence_number,
    uint16_t transport_wide_sequence_number) {
  rtc::CritScope cs(&lock_);
  if (mode_ != StorageMode::kStoreAndCull) {
    // We only care about transport wide sequence numbers if we are culling the
    // history using them.
    return;
  }

  auto rtp_it = packet_history_.find(rtp_sequence_number);
  if (rtp_it == packet_history_.end()) {
    // Non-retransmittable packet?
    return;
  }

  rtp_it->second.transport_sequence_number.emplace(
      transport_wide_sequence_number);

  auto it = tw_seqno_map_.emplace(transport_wide_sequence_number, rtp_it);
  RTC_DCHECK(it.second);
}

void RtpPacketHistory::OnTransportFeedback(
    const std::vector<PacketFeedback>& packet_feedback_vector) {
  rtc::CritScope cs(&lock_);
  if (mode_ != StorageMode::kStoreAndCull) {
    // We only care about transport wide sequence numbers if we are culling the
    // history using them.
    return;
  }

  for (const PacketFeedback& received_packet : packet_feedback_vector) {
    if (received_packet.arrival_time_ms == PacketFeedback::kNotReceived) {
      continue;
    }

    auto it = tw_seqno_map_.find(received_packet.sequence_number);
    if (it != tw_seqno_map_.end()) {
      // Found packet signaled as received by the remote end, remove it from the
      // packet history.
      RemovePacket(it->second);
    }
  }
}

void RtpPacketHistory::Reset() {
  tw_seqno_map_.clear();
  packet_history_.clear();
}

void RtpPacketHistory::CullOldPackets(int64_t now_ms) {
  int64_t packet_duration_ms =
      std::max(kMinPacketDurationRtt * rtt_ms_, kMinPacketDurationMs);
  while (!packet_history_.empty()) {
    auto stored_packet_it = packet_history_.find(*start_seqno_);

    if (packet_history_.size() >= kMaxCapacity) {
      // We have reached the absolute max capacity, remove one packet
      // unconditionally.
      RemovePacket(stored_packet_it);
      continue;
    }

    RTC_DCHECK(stored_packet_it != packet_history_.end());
    const StoredPacket& stored_packet = stored_packet_it->second;
    if (!stored_packet.send_time_ms) {
      // Don't remove packets that have not been sent.
      return;
    }

    if (*stored_packet.send_time_ms + packet_duration_ms > now_ms) {
      // Don't cull packets too early to avoid failed retransmission requests.
      return;
    }

    if (packet_history_.size() >= number_to_store_ ||
        (mode_ == StorageMode::kStoreAndCull && *stored_packet.send_time_ms &&
         *stored_packet.send_time_ms +
                 (packet_duration_ms * kPacketCullingDelayFactor) <=
             now_ms)) {
      // Too many packets in history, or this packet has timed out. Remove it
      // and continue.
      RemovePacket(stored_packet_it);
    } else {
      // No more packets can be removed right now.
      return;
    }
  }
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::RemovePacket(
    StoredPacketItr packet_it) {
  // Move the packet out from the StoredPacket container.
  std::unique_ptr<RtpPacketToSend> rtp_packet =
      std::move(packet_it->second.packet);

  // Erase any potential mapping from transport wide sequence number.
  if (packet_it->second.transport_sequence_number) {
    size_t erased =
        tw_seqno_map_.erase(*packet_it->second.transport_sequence_number);
    RTC_DCHECK_EQ(erased, 1u);
  }

  // Erase the packet from the map, and capture iterator to the next one.
  StoredPacketItr next_it = packet_history_.erase(packet_it);

  // |next_it| now points to the next element, or to the end. If the end,
  // check if we can wrap around.
  if (next_it == packet_history_.end()) {
    next_it = packet_history_.begin();
  }

  // Update |start_seq_no| to the new oldest item.
  if (next_it != packet_history_.end()) {
    start_seqno_.emplace(next_it->first);
  } else {
    start_seqno_.reset();
  }

  return rtp_packet;
}

}  // namespace webrtc
