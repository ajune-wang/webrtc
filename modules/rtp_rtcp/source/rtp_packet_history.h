/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_PACKET_HISTORY_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_PACKET_HISTORY_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class Clock;
class RtpPacketToSend;

class RtpPacketHistory {
 public:
  enum class StorageMode {
    kDisabled,     // Don't store any packets.
    kStore,        // Store & keep |number_to_store| packets, or more as needed.
    kStoreAndCull  // Store up to |number_to_store| packets, but try to remove
                   // packets as they time out or as signaled as received.
  };

  // If mode is kStore, |number_to_store| packets will be kept in memory. If the
  // oldest packet is less than |kMinPacketDurationMs| or
  // |kMinPacketDurationRtt| x RTT ms in age, keep it and allow the history to
  // grow up |kMaxCapacity|.
  //
  // If mode is kStoreAndCull, same as above applies, but packets may be
  // removed from the history packet is ACK'ed via TransportFeedback or is older
  // than |kMaxPacketDurationMs| (likely due to dropped feedback packet).
  // If ACK-culling is ENABLED, packets may additionally be culled after
  // |kPacketCullingDelayFactor| *
  //   min(|kMinPacketDurationMs|, |kMinPacketDurationRtt| * RTT) ms has passed.
  // This prevents packets from lingering too long if TransportFeedback packets
  // are lost.
  static constexpr size_t kMaxCapacity = 9600;
  static constexpr int64_t kMinPacketDurationMs = 1000;
  static constexpr int kMinPacketDurationRtt = 3;
  static constexpr int kPacketCullingDelayFactor = 3;

  // Snapshot indicating the state of a packet in the history. Used primarily
  // for retransmission, so that packets can be re-inserted into the pacer.
  // Also useful for testing purposes.
  struct PacketState {
    PacketState();
    PacketState(const PacketState&);
    ~PacketState();

    uint16_t rtp_sequence_number = 0;
    rtc::Optional<uint16_t> transport_sequence_number;
    // The time at which this packet was last sent, overwritten if packet is
    // retransmitted.
    rtc::Optional<int64_t> send_time_ms;
    int64_t capture_time_ms = 0;
    uint32_t ssrc = 0;
    size_t payload_size = 0;
    // Number of times RE-transmitted, ie not including the first transmission.
    size_t times_retransmitted = 0;
  };

  explicit RtpPacketHistory(Clock* clock);
  ~RtpPacketHistory();

  // Set/get storage mode. Note that setting the state will clear the history,
  // even if setting the same state as is currently used.
  void SetStorePacketsStatus(StorageMode mode, size_t number_to_store);
  StorageMode GetStorageMode() const;

  // Set RTT, used to avoid premature retransmission and to prevent over-writing
  // a packet in the history before we are reasonably sure it has been received.
  void SetRtt(int64_t rtt_ms);

  // If |send_time| is set, packet was sent without using pacer, so state will
  // be set accordingly.
  void PutRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                    StorageType type,
                    rtc::Optional<int64_t> send_time_ms);

  // Gets stored RTP packet corresponding to the input |sequence number|.
  // Returns nullptr if packet is not found.
  // If |verify_rtt| is true we verify that one RTT has elapsed since the last
  // time the packet was resent. If the packet is found but on RTT has has not
  // elapsed, returns nullptr without modifying the internal state. Otherwise
  // the time of last transmission is updated and the number of retransmissions
  // will be incremented if send time was already set.
  std::unique_ptr<RtpPacketToSend> GetPacketAndSetSendTime(
      uint16_t sequence_number,
      bool verify_rtt);

  // Similar to GetPacketAndSetSendTime(), but only returns a snapshot of the
  // current state for packet.
  // Return value will not have a value if sequence number was not found, or if
  // verify_rtt was set, and this the method was called too soon.
  rtc::Optional<PacketState> GetPacketState(uint16_t sequence_number,
                                            bool verify_rtt) const;

  // Get the packet (if any) from the history, with size closest to
  // |packet_size|. The exact size of the packet is not guaranteed.
  std::unique_ptr<RtpPacketToSend> GetBestFittingPacket(
      size_t packet_size) const;

  // Just before sending a packet on the network, a transport wide sequence
  // number may be set. Add this mapping so that and RTP packet created before
  // that time can be found when transport feedback is received.
  void OnTransportSequenceCreated(uint16_t rtp_sequence_number,
                                  uint16_t transport_wide_sequence_number);

  // When transport feedback is reported (and storage mode is StoreAndCull),
  // check the list of received transport sequence numbers and attempt to remove
  // them from the history.
  void OnTransportFeedback(
      const std::vector<PacketFeedback>& packet_feedback_vector);

 private:
  struct StoredPacket {
    explicit StoredPacket(std::unique_ptr<RtpPacketToSend> packet);
    StoredPacket(StoredPacket&&);
    StoredPacket& operator=(StoredPacket&&);
    ~StoredPacket();

    // Mapping as set by OnTransportSequenceCreated().
    rtc::Optional<uint16_t> transport_sequence_number;

    // The time at which this packet was last sent, overwritten if packet is
    // retransmitted.
    rtc::Optional<int64_t> send_time_ms;

    // Number of times RE-transmitted, ie excluding the first transmission.
    size_t times_retransmitted = 0;

    // Storing a packet with |storage_type| = kDontRetransmit indicates this is
    // only used as temporary storage until sent by the pacer sender.
    StorageType storage_type = kDontRetransmit;

    // The actual packet.
    std::unique_ptr<RtpPacketToSend> packet;
  };

  using StoredPacketIterator = std::map<uint16_t, StoredPacket>::iterator;

  // Helper method used by GetPacketAndSetSendTime() and GetPacketState().
  // Returns an iterator to the StoredPacket that should be used, or the end
  // iterator if not found or RTT check fails.
  bool VerifyRtt(const StoredPacket& packet, int64_t now_ms) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void Reset() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void CullOldPackets(int64_t now_ms) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Removes the packet from the history, and context/mapping that has been
  // stored. Returns the RTP packet instance contained within the StoredPacket.
  std::unique_ptr<RtpPacketToSend> RemovePacket(StoredPacketIterator packet)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  static PacketState StoredPacketToPacketState(
      const StoredPacket& stored_packet);

  Clock* const clock_;
  rtc::CriticalSection lock_;
  size_t number_to_store_ RTC_GUARDED_BY(lock_);
  StorageMode mode_ RTC_GUARDED_BY(lock_);
  int64_t rtt_ms_ RTC_GUARDED_BY(lock_);

  // Map from rtp sequence numbers to stored packet.
  std::map<uint16_t, StoredPacket> packet_history_ RTC_GUARDED_BY(lock_);

  // The earliest packet in the history. This might not be the lowest sequence
  // number, in case there is a wraparound.
  rtc::Optional<uint16_t> start_seqno_ RTC_GUARDED_BY(lock_);

  // Similar to |rtp_seqno_map_|, but indexed on transport wide sequence
  // numbers, as seen in the feedback packets used by send-side bwe. This will
  // allow us to cull packets that we know have already been recevied.
  std::map<uint16_t, StoredPacketIterator> tw_seqno_map_ RTC_GUARDED_BY(lock_);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtpPacketHistory);
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_PACKET_HISTORY_H_
