/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/remote_bitrate_estimator/congestion_control_feedback_generator.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/ecn_marking.h"

namespace webrtc {

namespace {
// RFC 3550, Section 4. Compact NTP.
//  Only the middle 32 bits are used; that is, the low 16
// bits of the integer part and the high 16 bits of the fractional part.
uint32_t CompactNtp(NtpTime ntp) {
  return (ntp.seconds() << 16) | (ntp.fractions() >> 16);
}
}  // namespace

CongestionControlFeedbackGenerator::CongestionControlFeedbackGenerator(
    const Environment& env,
    RtcpSender rtcp_sender)
    : env_(env),
      rtcp_sender_(
          std::move(rtcp_sender)),
          min_time_between_feedback_("min_send_delta", TimeDelta::Millis(25)),
          max_time_to_wait_for_packet_with_marker_("max_wait_for_marker",
                                                   TimeDelta::Millis(25)),
          max_time_between_feedback_("max_send_delta",
                                     TimeDelta::Millis(250)), ) {
  ParseFieldTrial(
      {&min_time_between_feedback_, &max_time_to_wait_for_packet_with_marker_,
       &max_time_between_feedback_},
      env.field_trials().Lookup("WebRTC-RFC8888CongestionControlFeedback"));
}

void CongestionControlFeedbackGenerator::OnReceivedPacket(
    const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  marker_bit_seen_ |= packet.Marker();
  PacketInfo info = {.ssrc = packet.Ssrc(),
                     .sequence_number = packet.SequenceNumber(),
                     .unwrapped_sequence_number =
                         sequence_number_unwrappers_[packet.Ssrc()].Unwrap(
                             packet.SequenceNumber()),
                     .arrival_time = packet.arrival_time(),
                     .ecn = packet.ecn()};
  packets_.push_back(info);
  if (TimeToSendFeedback(packet.arrival_time()).IsZero()) {
    SendFeedback(env_.clock().CurrentTime());
  }
}

TimeDelta CongestionControlFeedbackGenerator::TimeToSendFeedback(
    Timestamp now) const {
  Timestamp next_send_time = last_feedback_sent_time_ + time_to_next_process_;
  if (next_send_time > now) {
    return next_send_time - now;
  }
  if (packets_.empty()) {
    return min_time_between_feedback_;
  }
  if (!marker_bit_seen_ && (now - packets_.back().arrival_time) <
                               max_time_to_wait_for_packet_with_marker_) {
    return max_time_to_wait_for_packet_with_marker_.Get() -
           (now - packets_.back().arrival_time);
  }
  return TimeDelta::Zero();
}

TimeDelta CongestionControlFeedbackGenerator::Process(Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  TimeDelta next_feedback = TimeToSendFeedback(now);
  if (next_feedback.IsZero()) {
    SendFeedback(now);
    return time_to_next_process_;
  }
  return next_feedback;
}

void CongestionControlFeedbackGenerator::OnSendBandwidthEstimateChanged(
    DataRate estimate) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // Feedback reports should max occupy 5% of total bandwidth.
  max_feedback_rate_ = estimate * 0.05;
}

void CongestionControlFeedbackGenerator::SetTransportOverhead(
    DataSize overhead_per_packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  packet_overhead_ = overhead_per_packet;
}

void CongestionControlFeedbackGenerator::SendFeedback(Timestamp now) {
  std::sort(packets_.begin(), packets_.end(),
            [](const PacketInfo& a, const PacketInfo& b) {
              if (a.ssrc == b.ssrc) {
                return a.unwrapped_sequence_number <
                       b.unwrapped_sequence_number;
              }
              return a.ssrc < b.ssrc;
            });
  uint32_t compact_ntp =
      CompactNtp(env_.clock().ConvertTimestampToNtpTime(now));
  std::vector<rtcp::CongestionControlFeedback::PacketInfo> rtcp_packet_info;
  rtcp_packet_info.reserve(packets_.size());

  absl::optional<uint32_t> previous_ssrc;
  absl::optional<int64_t> previous_seq_no;
  for (const PacketInfo packet : packets_) {
    if (previous_ssrc == packet.ssrc &&
        previous_seq_no == packet.unwrapped_sequence_number) {
      // According to RFC 8888:
      // If duplicate copies of a particular RTP packet are received, then the
      // arrival time of the first copy to arrive MUST be reported. If any of
      // the copies of the duplicated packet are ECN-CE marked, then an ECN-CE
      // mark MUST be reported for that packet; otherwise, the ECN mark of the
      // first copy to arrive is reported.
      if (packet.ecn == rtc::EcnMarking::kCe) {
        rtcp_packet_info.back().ecn = packet.ecn;
      }
      RTC_LOG(LS_WARNING) << "Received duplicate packet ssrc:" << packet.ssrc
                          << " seq:" << packet.sequence_number;
    } else {
      previous_ssrc = packet.ssrc;
      previous_seq_no = packet.unwrapped_sequence_number;
      rtcp_packet_info.push_back(
          {.ssrc = packet.ssrc,
           .sequence_number = packet.sequence_number,
           .arrival_time_offset = now - packet.arrival_time,
           .ecn = packet.ecn});
    }
  }
  packets_.clear();
  marker_bit_seen_ = false;

  auto feedback = std::make_unique<rtcp::CongestionControlFeedback>(
      std::move(rtcp_packet_info), compact_ntp);
  CalculateNextPossibleSendTime(DataSize::Bytes(feedback->BlockLength()), now);

  std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets;
  rtcp_packets.push_back(std::move(feedback));
  rtcp_sender_(std::move(rtcp_packets));
}

void CongestionControlFeedbackGenerator::CalculateNextPossibleSendTime(
    DataSize feedback_size,
    Timestamp now) {
  TimeDelta time_since_last_sent = now - last_feedback_sent_time_;
  DataSize decreased_debt = time_since_last_sent * max_feedback_rate_;
  send_rate_debt_ = decreased_debt > send_rate_debt_
                        ? DataSize::Zero()
                        : send_rate_debt_ - decreased_debt;
  send_rate_debt_ += feedback_size + packet_overhead_;
  last_feedback_sent_time_ = now;
  time_to_next_process_ = std::clamp(send_rate_debt_ / max_feedback_rate_,
                                     min_time_between_feedback_.Get(),
                                     max_time_between_feedback_.Get());
}

}  // namespace webrtc
