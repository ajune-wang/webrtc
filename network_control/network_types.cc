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

#include <algorithm>
#include "network_control/include/network_message.h"
#include "rtc_base/checks.h"

using webrtc::network::units::Timestamp;
using webrtc::network::units::DataSize;

namespace webrtc {
namespace network {

namespace {
template <typename vec_t, typename P>
bool Contains(const vec_t& vec, const P pred) {
  return std::find_if(vec.begin(), vec.end(), pred) != vec.end();
}
}  // namespace

template struct signal::Message<SentPacket>;
template struct signal::Message<StreamsConfig>;
template struct signal::Message<TransportPacketsFeedback>;
template struct signal::Message<TransportLossReport>;
template struct signal::Message<RoundTripTimeReport>;
template struct signal::Message<RemoteBitrateReport>;
template struct signal::Message<TargetRateConstraints>;
template struct signal::Message<NetworkAvailability>;
template struct signal::Message<NetworkRouteChange>;
template struct signal::Message<NetworkEstimate>;
template struct signal::Message<TargetTransferRate>;
template struct signal::Message<PacerConfig>;
template struct signal::Message<PacerState>;
template struct signal::Message<CongestionWindow>;
template struct signal::Message<ProbeClusterConfig>;
template struct signal::Message<ProcessInterval>;
template struct signal::Message<Invalidation>;

::std::ostream& operator<<(::std::ostream& os,
                           const ProbeClusterConfig& config) {
  return os << "ProbeClusterConfig(...)";
}

::std::ostream& operator<<(::std::ostream& os, const PacerConfig& config) {
  return os << "PacerConfig(...)";
}

uint8_t LossRate::GetLossRatioUint8() {
  int loss_ratio_255 = loss_ratio * 255;
  if (loss_ratio_255 < 0)
    return 0;
  else if (loss_ratio_255 > 255)
    return 255;
  else
    return (uint8_t)loss_ratio_255;
}

SimplePacketFeedback::SimplePacketFeedback(const webrtc::PacketFeedback& pf) {
  if (pf.arrival_time_ms != webrtc::PacketFeedback::kNotReceived)
    receive_time = Timestamp::ms(pf.arrival_time_ms);
  if (pf.send_time_ms != webrtc::PacketFeedback::kNoSendTime) {
    sent_packet = SentPacket();
    sent_packet->send_time = Timestamp::ms(pf.send_time_ms);
    sent_packet->size = DataSize::bytes(pf.payload_size);
    sent_packet->pacing_info = pf.pacing_info;
  }
}

SimplePacketFeedback::SimplePacketFeedback() {}

SimplePacketFeedback::SimplePacketFeedback(const SimplePacketFeedback& other) {
  sent_packet = other.sent_packet;
  receive_time = other.receive_time;
}

SimplePacketFeedback::~SimplePacketFeedback() {}

SimplePacketFeedback SimplePacketFeedback::FromRtpPacketFeedback(
    const PacketFeedback& rtp_packet_feedback) {
  return SimplePacketFeedback(rtp_packet_feedback);
}

TransportPacketsFeedback::TransportPacketsFeedback() {}
TransportPacketsFeedback::TransportPacketsFeedback(
    const TransportPacketsFeedback& other)
    : feedback_time(other.feedback_time),
      packet_feedbacks(other.packet_feedbacks) {}

TransportPacketsFeedback::TransportPacketsFeedback(
    const std::vector<PacketFeedback>& feedback_vector,
    int64_t creation_time_ms) {
  RTC_DCHECK(std::is_sorted(feedback_vector.begin(), feedback_vector.end(),
                            PacketFeedbackComparator()));
  feedback_time = Timestamp::ms(creation_time_ms);
  packet_feedbacks.reserve(feedback_vector.size());
  for (const PacketFeedback& rtp_feedback : feedback_vector) {
    auto feedback = SimplePacketFeedback::FromRtpPacketFeedback(rtp_feedback);
    packet_feedbacks.push_back(feedback);
  }
}

TransportPacketsFeedback TransportPacketsFeedback::FromRtpFeedbackVector(
    const std::vector<PacketFeedback>& feedback_vector,
    int64_t creation_time_ms) {
  return TransportPacketsFeedback(feedback_vector, creation_time_ms);
}

TransportPacketsFeedback TransportPacketsFeedback::FromRtpFeedbackVector(
    const std::vector<PacketFeedback>& feedback_vector) {
  const PacketFeedback& first = feedback_vector.front();
  const PacketFeedback& last = feedback_vector.back();
  RTC_DCHECK_EQ(first.creation_time_ms, last.creation_time_ms);
  return TransportPacketsFeedback(feedback_vector, first.creation_time_ms);
}
TransportPacketsFeedback::~TransportPacketsFeedback() {}
}  // namespace network
}  // namespace webrtc
