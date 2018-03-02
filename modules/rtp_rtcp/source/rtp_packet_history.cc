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

RtpPacketHistory::StoredPacket::StoredPacket() = default;
RtpPacketHistory::StoredPacket::StoredPacket(StoredPacket&&) = default;
RtpPacketHistory::StoredPacket& RtpPacketHistory::StoredPacket::operator=(
    RtpPacketHistory::StoredPacket&&) = default;
RtpPacketHistory::StoredPacket::~StoredPacket() = default;

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
  auto stored_packet = packet_history_.emplace(packet_history_.end());
  if (packet->capture_time_ms() <= 0) {
    packet->set_capture_time_ms(now_ms);
  }
  stored_packet->rtp_sequence_number = packet->SequenceNumber();
  stored_packet->send_time_ms = send_time;
  stored_packet->storage_type = type;
  stored_packet->times_retransmitted = 0;
  stored_packet->packet = std::move(packet);

  // Add references from sequence numbers to stored packet.
  RTC_DCHECK(rtp_seqno_map_.find(stored_packet->rtp_sequence_number) ==
             rtp_seqno_map_.end());
  rtp_seqno_map_.emplace(stored_packet->rtp_sequence_number, stored_packet);

  bytes_in_history += stored_packet->packet->size();

  if (stored_packet->rtp_sequence_number % 100 == 0) {
    printf("%lu: Currently %lu payload bytes in packet history.\n",
           reinterpret_cast<uint64_t>(this), bytes_in_history);
  }
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPacket(
    uint16_t sequence_number,
    bool verify_rtt,
    bool update_send_state) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return std::unique_ptr<RtpPacketToSend>();
  }

  auto rtp_it = rtp_seqno_map_.find(sequence_number);
  if (rtp_it == rtp_seqno_map_.end()) {
    // Packet not found.
    printf("@No match for rtp seq# %u\n", sequence_number);
    return std::unique_ptr<RtpPacketToSend>();
  }

  int64_t now_ms = clock_->TimeInMilliseconds();
  StoredPacket& stored_packet = *rtp_it->second;
  if (stored_packet.storage_type == StorageType::kDontRetransmit) {
    // Non retransmittable packet, so call must come from paced sender.
    // Remove from history and return actual packet instance.
    // printf("@NoRetrans, removing %u\n", stored_packet.rtp_sequence_number);
    return RemovePacket(rtp_it->second);
  }

  if (stored_packet.send_time_ms) {
    // Send-time already set, this must be a retransmission.
    if (verify_rtt && stored_packet.times_retransmitted > 0 &&
        now_ms < *stored_packet.send_time_ms + rtt_ms_) {
      // This packet has already been retransmitted once, and the time since
      // that even is lower than on RTT. Ignore request as this packet is
      // likely already in the network pipe.
      return std::unique_ptr<RtpPacketToSend>();
    }

    if (update_send_state) {
      ++stored_packet.times_retransmitted;
    }
  }

  if (update_send_state) {
    // Update send-time and return copy of packet instance.
    stored_packet.send_time_ms.emplace(now_ms);
  }
  return rtc::MakeUnique<RtpPacketToSend>(*stored_packet.packet);
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
    size_t diff = SizeDiff(it.packet, packet_length);
    if (!min_diff || diff < *min_diff) {
      min_diff.emplace(diff);
      best_packet = it.packet.get();
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

  auto rtp_it = rtp_seqno_map_.find(rtp_sequence_number);
  if (rtp_it == rtp_seqno_map_.end()) {
    // Non-retransmittable packet?
    return;
  }

  auto stored_packet = rtp_it->second;
  stored_packet->transport_sequence_number.emplace(
      transport_wide_sequence_number);

  auto it =
      tw_seqno_map_.emplace(transport_wide_sequence_number, stored_packet);
  RTC_DCHECK(it.second);

  //  printf("%lu: Added TW seqno mapping: tw seq# %u / rtp seq# %u .\n",
  //          reinterpret_cast<uint64_t>(this),
  //          transport_wide_sequence_number,
  //          rtp_sequence_number);
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
      //      printf("%lu: PacketFeedback received, removing tw seq# %u / rtp
      //      seq# %u, arrival time %ld .\n",
      //              reinterpret_cast<uint64_t>(this),
      //              received_packet.sequence_number,
      //              it->second->rtp_sequence_number,
      //              received_packet.arrival_time_ms);
      RemovePacket(it->second);
    }
  }
}

void RtpPacketHistory::Reset() {
  tw_seqno_map_.clear();
  rtp_seqno_map_.clear();
  packet_history_.clear();
}

void RtpPacketHistory::CullOldPackets(int64_t now_ms) {
  int64_t packet_duration_ms =
      std::max(kMinPacketDurationRtt * rtt_ms_, kMinPacketDurationMs);
  while (!packet_history_.empty()) {
    if (packet_history_.size() >= kMaxCapacity) {
      // We have reached the absolute max capacity, remove one packet
      // unconditionally.
      abort();
      RemovePacket(packet_history_.begin());
      continue;
    }

    auto stored_packet = packet_history_.begin();
    if (!stored_packet->send_time_ms) {
      // Don't remove packets that have not been sent.
      return;
    }
    if (*stored_packet->send_time_ms + packet_duration_ms > now_ms) {
      // Don't cull packets too early to avoid failed retransmission requests.
      return;
    }

    if (packet_history_.size() >= number_to_store_ ||
        (mode_ == StorageMode::kStoreAndCull && *stored_packet->send_time_ms &&
         *stored_packet->send_time_ms + (packet_duration_ms * 3) <= now_ms)) {
      // Too many packets in history, or this packet has timed out. Remove it
      // and continue.
      //      printf("@Too old, culling rtp seq# %u, hist %lu, max hist %lu, "
      //             "send time %ld, duration %ld, now %ld\n",
      //             stored_packet->rtp_sequence_number,
      //             packet_history_.size(), number_to_store_,
      //             *stored_packet->send_time_ms,
      //             packet_duration_ms * 3,
      //             now_ms);
      RemovePacket(stored_packet);
    } else {
      // No more packets can be removed right now.
      return;
    }
  }
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::RemovePacket(
    std::list<RtpPacketHistory::StoredPacket>::iterator packet) {
  std::unique_ptr<RtpPacketToSend> rtp_packet = std::move(packet->packet);
  if (packet->transport_sequence_number) {
    size_t erased = tw_seqno_map_.erase(*packet->transport_sequence_number);
    RTC_DCHECK_EQ(erased, 1u);
  }
  size_t erased = rtp_seqno_map_.erase(packet->rtp_sequence_number);
  RTC_DCHECK_EQ(erased, 1u);
  packet_history_.erase(packet);
  bytes_in_history -= rtp_packet->size();
  return rtp_packet;
}

}  // namespace webrtc
