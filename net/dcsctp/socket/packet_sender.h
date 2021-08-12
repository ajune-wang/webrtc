/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_PACKET_SENDER_H_
#define NET_DCSCTP_SOCKET_PACKET_SENDER_H_

#include <list>
#include <memory>
#include <vector>

#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/timer/timer.h"

namespace dcsctp {

// The PacketSender sends packets to the network using the provided callback
// interface. When an attempt to send a packet is made, the `on_sent_packet`
// callback will be triggered.
class PacketSender {
 public:
  PacketSender(TimerManager& timer_manager,
               DcSctpSocketCallbacks& callbacks,
               std::function<void(rtc::ArrayView<const uint8_t> packet,
                                  SendPacketStatus)> on_sent_packet);

  // Should be (but doesn't have to be) called prior to sending any packets in
  // bulk. If there are packets in the retry queue, these will be sent, and if
  // all of them were sent, this method returns true. If there were no packets
  // in the queue, this method returns true. If this method returns false, it
  // means that it didn't manage to send all queued packets, so no new packets
  // should be sent.
  bool PrepareToSend() { return RetrySendPackets(); }

  // Sends the packet, and returns true if it was sent successfully. If sending
  // the packet resulted in a temporary failure, the packet will be queued and
  // will be scheduled for retransmission soon after, and false will be
  // returned.
  bool Send(SctpPacket::Builder& builder);

 private:
  absl::optional<DurationMs> OnRetryTimerExpiry();
  bool RetrySendPackets();

  DcSctpSocketCallbacks& callbacks_;

  // Callback that will be triggered for every send attempt, indicating the
  // status of the operation.
  std::function<void(rtc::ArrayView<const uint8_t>, SendPacketStatus)>
      on_sent_packet_;

  // Retries to send packets (in `retry_queue_`) that failed to be sent
  // earlier.
  const std::unique_ptr<Timer> retry_timer_;

  // Packets that failed to be sent, that will be prioritized to be sent next
  // time a packet is due. When there are packets here, the `retry_queue_timer_`
  // is running, which tries to resend these packets once expired. But they may
  // also be sent in other situations.
  std::list<std::vector<uint8_t>> retry_queue_;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_SOCKET_PACKET_SENDER_H_
