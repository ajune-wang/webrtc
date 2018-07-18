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

// PCC divides time into consecutive monitor intervals which are used to test
// consequences for performance of sending at a certain rate.
MonitorInterval::MonitorInterval(DataRate target_sending_rate,
                                 Timestamp start_time,
                                 TimeDelta duration)
    : target_sending_rate_(target_sending_rate),
      start_time_(start_time),
      interval_duration_(duration),
      received_packets_size_(DataSize::Zero()),
      lost_packets_size_(DataSize::Zero()),
      feedback_collection_done_(false) {}

MonitorInterval::~MonitorInterval() = default;

MonitorInterval::MonitorInterval(const MonitorInterval& other) = default;

void MonitorInterval::OnPacketsFeedback(
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
      received_packets_rtt_.push_back(packet_result.receive_time -
                                      packet_result.sent_packet->send_time);
      received_packets_size_ += packet_result.sent_packet->size;
      received_packets_sent_time_.push_back(
          packet_result.sent_packet->send_time);
    }
  }
}

bool MonitorInterval::IsFeedbackCollectionDone() const {
  return feedback_collection_done_;
}

Timestamp MonitorInterval::GetEndTime() const {
  return start_time_ + interval_duration_;
}

double MonitorInterval::GetLossRate() const {
  return lost_packets_size_ / (lost_packets_size_ + received_packets_size_);
}

DataRate MonitorInterval::GetTargetSendingRate() const {
  return target_sending_rate_;
}

const std::vector<TimeDelta>& MonitorInterval::GetReceivedPacketsRtt() const {
  return received_packets_rtt_;
}

const std::vector<Timestamp>& MonitorInterval::GetReceivedPacketsSentTime()
    const {
  return received_packets_sent_time_;
}

const std::vector<Timestamp>& MonitorInterval::GetLostPacketsSentTime() const {
  return lost_packets_sent_time_;
}

}  // namespace pcc
}  // namespace webrtc
