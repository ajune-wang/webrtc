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
namespace network {

template struct signal::Message<CongestionWindow>;
template struct signal::Message<NetworkAvailability>;
template struct signal::Message<NetworkEstimate>;
template struct signal::Message<NetworkRouteChange>;
template struct signal::Message<OutstandingData>;
template struct signal::Message<PacerConfig>;
template struct signal::Message<ProbeClusterConfig>;
template struct signal::Message<PacerQueueUpdate>;
template struct signal::Message<ProcessInterval>;
template struct signal::Message<RemoteBitrateReport>;
template struct signal::Message<RoundTripTimeReport>;
template struct signal::Message<SentPacket>;
template struct signal::Message<StreamsConfig>;
template struct signal::Message<TargetRateConstraints>;
template struct signal::Message<TargetTransferRate>;
template struct signal::Message<TransportLossReport>;
template struct signal::Message<TransportPacketsFeedback>;

::std::ostream& operator<<(::std::ostream& os,
                           const ProbeClusterConfig& config) {
  return os << "ProbeClusterConfig(...)";
}

::std::ostream& operator<<(::std::ostream& os, const PacerConfig& config) {
  return os << "PacerConfig(...)";
}

NetworkPacketFeedback::NetworkPacketFeedback() {}

NetworkPacketFeedback::NetworkPacketFeedback(
    const NetworkPacketFeedback& other) {
  sent_packet = other.sent_packet;
  receive_time = other.receive_time;
}

NetworkPacketFeedback::~NetworkPacketFeedback() {}

std::vector<NetworkPacketFeedback>
TransportPacketsFeedback::ReceivedWithHistory() {
  std::vector<NetworkPacketFeedback> res;
  for (NetworkPacketFeedback& fb : packet_feedbacks) {
    if (fb.receive_time.has_value() && fb.sent_packet.has_value()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<NetworkPacketFeedback> TransportPacketsFeedback::LostWithHistory() {
  std::vector<NetworkPacketFeedback> res;
  for (NetworkPacketFeedback& fb : packet_feedbacks) {
    if (!fb.receive_time.has_value() && fb.sent_packet.has_value()) {
      res.push_back(fb);
    }
  }
  return res;
}

TransportPacketsFeedback::TransportPacketsFeedback() {}
TransportPacketsFeedback::TransportPacketsFeedback(
    const TransportPacketsFeedback& other)
    : feedback_time(other.feedback_time),
      data_in_flight(other.data_in_flight),
      packet_feedbacks(other.packet_feedbacks) {}

TransportPacketsFeedback::~TransportPacketsFeedback() {}

uint8_t NetworkEstimate::GetLossRatioUint8() {
  int loss_ratio_255 = loss_rate_ratio * 255;
  if (loss_ratio_255 < 0)
    return 0;
  else if (loss_ratio_255 > 255)
    return 255;
  else
    return (uint8_t)loss_ratio_255;
}
}  // namespace network
}  // namespace webrtc
