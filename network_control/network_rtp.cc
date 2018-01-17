/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "network_control/include/network_rtp.h"

#include <algorithm>
#include "rtc_base/checks.h"

namespace webrtc {

NetworkPacketFeedback NetworkPacketFeedbackFromRtpPacketFeedback(
    const webrtc::PacketFeedback& pf) {
  NetworkPacketFeedback feedback;
  if (pf.arrival_time_ms != webrtc::PacketFeedback::kNotReceived)
    feedback.receive_time = Timestamp::ms(pf.arrival_time_ms);
  if (pf.send_time_ms != webrtc::PacketFeedback::kNoSendTime) {
    feedback.sent_packet = SentPacket();
    feedback.sent_packet->send_time = Timestamp::ms(pf.send_time_ms);
    feedback.sent_packet->size = DataSize::bytes(pf.payload_size);
    feedback.sent_packet->pacing_info = pf.pacing_info;
  }
  return feedback;
}

TransportPacketsFeedback TransportPacketsFeedbackFromRtpFeedbackVector(
    const std::vector<PacketFeedback>& feedback_vector,
    int64_t creation_time_ms) {
  TransportPacketsFeedback tpf;
  RTC_DCHECK(std::is_sorted(feedback_vector.begin(), feedback_vector.end(),
                            PacketFeedbackComparator()));
  tpf.feedback_time = Timestamp::ms(creation_time_ms);
  tpf.packet_feedbacks.reserve(feedback_vector.size());
  for (const PacketFeedback& rtp_feedback : feedback_vector) {
    auto feedback = NetworkPacketFeedbackFromRtpPacketFeedback(rtp_feedback);
    tpf.packet_feedbacks.push_back(feedback);
  }
  return tpf;
}

TransportPacketsFeedback TransportPacketsFeedbackFromRtpFeedbackVector(
    const std::vector<PacketFeedback>& feedback_vector) {
  const PacketFeedback& first = feedback_vector.front();
  const PacketFeedback& last = feedback_vector.back();
  RTC_DCHECK_EQ(first.creation_time_ms, last.creation_time_ms);
  return TransportPacketsFeedbackFromRtpFeedbackVector(feedback_vector,
                                                       first.creation_time_ms);
}
}  // namespace webrtc
