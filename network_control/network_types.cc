/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "network_control/include/network_types.h"

namespace webrtc {

template struct SignalMessage<CongestionWindow>;
template struct SignalMessage<NetworkAvailability>;
template struct SignalMessage<NetworkEstimate>;
template struct SignalMessage<NetworkRouteChange>;
template struct SignalMessage<OutstandingData>;
template struct SignalMessage<PacerConfig>;
template struct SignalMessage<ProbeClusterConfig>;
template struct SignalMessage<PacerQueueUpdate>;
template struct SignalMessage<ProcessInterval>;
template struct SignalMessage<RemoteBitrateReport>;
template struct SignalMessage<RoundTripTimeReport>;
template struct SignalMessage<SentPacket>;
template struct SignalMessage<StreamsConfig>;
template struct SignalMessage<TargetRateConstraints>;
template struct SignalMessage<TargetTransferRate>;
template struct SignalMessage<TransportLossReport>;
template struct SignalMessage<TransportPacketsFeedback>;

::std::ostream& operator<<(::std::ostream& os,
                           const ProbeClusterConfig& config) {
  return os << "ProbeClusterConfig(...)";
}

::std::ostream& operator<<(::std::ostream& os, const PacerConfig& config) {
  return os << "PacerConfig(...)";
}

PacketResult::PacketResult() {}

PacketResult::PacketResult(const PacketResult& other) = default;

PacketResult::~PacketResult() {}

std::vector<PacketResult> TransportPacketsFeedback::ReceivedWithSendInfo()
    const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (fb.receive_time.IsFinite() && fb.sent_packet.has_value()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<PacketResult> TransportPacketsFeedback::LostWithSendInfo() const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (fb.receive_time.IsInfinite() && fb.sent_packet.has_value()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<PacketResult> TransportPacketsFeedback::PacketsWithFeedback()
    const {
  return packet_feedbacks;
}

TransportPacketsFeedback::TransportPacketsFeedback() {}

TransportPacketsFeedback::TransportPacketsFeedback(
    const TransportPacketsFeedback& other) = default;

TransportPacketsFeedback::~TransportPacketsFeedback() {}

}  // namespace webrtc
