/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/pcc/monitor_interval.h"

namespace webrtc {
namespace pcc {

PccMonitorInterval::PccMonitorInterval(DataRate target_sending_rate,
                                 Timestamp start_time,
                                 TimeDelta duration)
    : target_sending_rate_(target_sending_rate),
      start_time_(start_time),
      interval_duration_(duration),
      received_packets_size_(DataSize::Zero()),
      lost_packets_size_(DataSize::Zero()),
      feedback_collection_done_(false) {}

PccMonitorInterval::~PccMonitorInterval() = default;

PccMonitorInterval::PccMonitorInterval(const PccMonitorInterval& other) = default;

void PccMonitorInterval::OnPacketsFeedback(
    const std::vector<PacketResult>& packets_results) {
  for (const PacketResult& packet_result : packets_results) {
    if (!packet_result.sent_packet.has_value() ||
        packet_result.sent_packet->send_time <= start_time_) {
      continue;
    }
    if (packet_result.sent_packet->send_time >
        start_time_ + interval_duration_) {
      feedback_collection_done_ = true;
      return;
    }
    if (packet_result.receive_time.IsInfinite()) {
      lost_packets_size_ += packet_result.sent_packet->size;
      lost_packets_sent_time_.push_back(packet_result.sent_packet->send_time);
    } else {
      received_packets_.push_back({packet_result.receive_time -
                                   packet_result.sent_packet->send_time,
                                   packet_result.sent_packet->send_time});
      received_packets_size_ += packet_result.sent_packet->size;
    }
  }
}

double PccMonitorInterval::ComputeDelayGradient(
    double delay_gradient_threshold) const {
  if (received_packets_.empty() ||
      received_packets_.front().sent_time ==
          received_packets_.back().sent_time) {
    return 0;
  }
  double sum_times = 0;
  double sum_delays = 0;
  double sum_square_times = 0;
  double sum_times_dot_delays = 0;
  for (const ReceivedPacket& packet : received_packets_) {
    double time = (packet.sent_time - received_packets_[0].sent_time).us();
    double delay = packet.delay.us();
    sum_times += time;
    sum_delays += delay;
    sum_square_times += time * time;
    sum_times_dot_delays += delay * time;
  }
  double rtt_gradient =
      (received_packets_.size() * sum_times_dot_delays -
       sum_delays * sum_times) /
      (received_packets_.size() * sum_square_times - sum_times * sum_times);
  rtt_gradient =
      (std::abs(rtt_gradient) < delay_gradient_threshold) ? 0 : rtt_gradient;
  return rtt_gradient;
}

bool PccMonitorInterval::IsFeedbackCollectionDone() const {
  return feedback_collection_done_;
}

Timestamp PccMonitorInterval::GetEndTime() const {
  return start_time_ + interval_duration_;
}

double PccMonitorInterval::GetLossRate() const {
  if (lost_packets_size_.IsZero() && received_packets_size_.IsZero()) {
    return 0;
  }
  return lost_packets_size_ / (lost_packets_size_ + received_packets_size_);
}

DataRate PccMonitorInterval::GetTargetSendingRate() const {
  return target_sending_rate_;
}

DataRate PccMonitorInterval::GetTransmittedPacketsRate() const {
  Timestamp receive_time_of_first_packet =
      received_packets_.front().sent_time + received_packets_.front().delay;
  Timestamp receive_time_of_last_packet =
      received_packets_.back().sent_time + received_packets_.back().delay;
  return received_packets_size_ /
         (receive_time_of_last_packet - receive_time_of_first_packet);
}

}  // namespace pcc
}  // namespace webrtc
