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

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace webrtc {
namespace pcc {

MonitorInterval::MonitorInterval(DataRate target_sending_rate,
                                 Timestamp start_time,
                                 TimeDelta duration)
    : target_sending_rate_{target_sending_rate},
      start_time_{start_time},
      duration_{duration},
      received_packets_size_{DataSize::Zero()},
      lost_packets_size_{DataSize::Zero()},
      got_complete_feedback_{false} {}

MonitorInterval::~MonitorInterval() = default;

MonitorInterval::MonitorInterval(const MonitorInterval& other) = default;

bool MonitorInterval::OnPacketsFeedback(
    const std::vector<PacketResult>& packets_results) {
  for (const PacketResult& packet_result : packets_results) {
    if (!packet_result.sent_packet.has_value()) {
      continue;
    }
    if (packet_result.sent_packet->send_time <= start_time_) {
      continue;
    }
    if (packet_result.sent_packet->send_time > start_time_ + duration_) {
      got_complete_feedback_ = true;
      return true;
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
  return false;
}

bool MonitorInterval::IsFeedbackCollectingDone() const {
  return got_complete_feedback_;
}

Timestamp MonitorInterval::GetEndTime() const {
  return start_time_ + duration_;
}

TimeDelta MonitorInterval::GetIntervalDuration() const {
  return duration_;
}

DataRate MonitorInterval::GetTargetBitrate() const {
  return target_sending_rate_;
}

std::vector<TimeDelta> MonitorInterval::GetReceivedPacketsRtt() const {
  return received_packets_rtt_;
}

std::vector<Timestamp> MonitorInterval::GetReceivedPacketsSentTime() const {
  return received_packets_sent_time_;
}

std::vector<Timestamp> MonitorInterval::GetLostPacketsSentTime() const {
  return lost_packets_sent_time_;
}

double MonitorInterval::GetLossRate() const {
  return lost_packets_size_ / (lost_packets_size_ + received_packets_size_);
}

MonitorBlock::MonitorBlock(Timestamp current_time,
                           TimeDelta intervals_duration,
                           DataRate default_bitrate,
                           TimeDelta mi_timeout,
                           std::vector<DataRate> monitor_intervals_bitrates)
    : last_known_time_{current_time},
      monitor_intervals_bitrates_{monitor_intervals_bitrates},
      intervals_duration_{intervals_duration},
      default_bitrate_{default_bitrate},
      mi_timeout_{mi_timeout},
      complete_feedback_mi_number_{0},
      got_complete_feedback_{false} {
  if (!monitor_intervals_bitrates.empty()) {
    monitor_intervals_.emplace_back(monitor_intervals_bitrates_[0],
                                    current_time, intervals_duration);
  }
}

MonitorBlock::~MonitorBlock() = default;

DataRate MonitorBlock::NotifyCurrentTime(Timestamp time) {
  last_known_time_ = time;
  if (Size() == 0) {
    return default_bitrate_;
  }
  if (time < monitor_intervals_.back().GetEndTime()) {
    return monitor_intervals_.back().GetTargetBitrate();
  }
  if (monitor_intervals_.size() >= monitor_intervals_bitrates_.size()) {
    return default_bitrate_;
  }
  monitor_intervals_.emplace_back(
      monitor_intervals_bitrates_[monitor_intervals_.size()], time,
      intervals_duration_);
  return monitor_intervals_.back().GetTargetBitrate();
}

DataRate MonitorBlock::GetTargetBirtate() const {
  if (monitor_intervals_.size() < monitor_intervals_bitrates_.size() ||
      last_known_time_ < monitor_intervals_.back().GetEndTime()) {
    return monitor_intervals_.back().GetTargetBitrate();
  }
  return default_bitrate_;
}

bool MonitorBlock::OnPacketsFeedback(
    const std::vector<PacketResult>& packets_results) {
  if (got_complete_feedback_) {
    return got_complete_feedback_;
  }
  if (monitor_intervals_.empty()) {
    got_complete_feedback_ = true;
    return got_complete_feedback_;
  }
  if (complete_feedback_mi_number_ < monitor_intervals_.size() &&
      monitor_intervals_[complete_feedback_mi_number_].OnPacketsFeedback(
          packets_results)) {
    ++complete_feedback_mi_number_;
  }
  if (complete_feedback_mi_number_ == monitor_intervals_bitrates_.size())
    got_complete_feedback_ = true;
  return got_complete_feedback_;
}

void MonitorBlock::UpdateTimeout(TimeDelta new_timeout) {
  mi_timeout_ = new_timeout;
}

bool MonitorBlock::IsFeedbackCollectingDone() const {
  return got_complete_feedback_;
}

bool MonitorBlock::IsTimeoutExpired() const {
  if (complete_feedback_mi_number_ >= monitor_intervals_.size()) {
    return false;
  }
  return last_known_time_ -
             monitor_intervals_[complete_feedback_mi_number_].GetEndTime() >=
         mi_timeout_;
}

size_t MonitorBlock::Size() const {
  return monitor_intervals_bitrates_.size();
}

const MonitorInterval* MonitorBlock::GetMonitorInterval(size_t index) const {
  if (index >= monitor_intervals_.size()) {
    return nullptr;
  }
  return &monitor_intervals_[index];
}

}  // namespace pcc
}  // namespace webrtc
