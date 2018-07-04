/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_PCC_MONITOR_INTERVAL_H_
#define MODULES_CONGESTION_CONTROLLER_PCC_MONITOR_INTERVAL_H_

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
namespace pcc {

class MonitorInterval {
 public:
  MonitorInterval(DataRate target_sending_rate,
                  Timestamp start_time,
                  TimeDelta duration);
  ~MonitorInterval();
  MonitorInterval(const MonitorInterval& other);
  // Returns true if got complete information about packets.
  // To understand this there should be packet sent after end of the MI since MI
  // doesn't save information about sent packets.
  bool OnPacketsFeedback(const std::vector<PacketResult>& packets_results);
  bool IsFeedbackCollectingDone() const;

  Timestamp GetEndTime() const;
  TimeDelta GetIntervalDuration() const;
  DataRate GetTargetBitrate() const;

  std::vector<TimeDelta> GetReceivedPacketsRtt() const;
  std::vector<Timestamp> GetReceivedPacketsSentTime() const;
  std::vector<Timestamp> GetLostPacketsSentTime() const;
  double GetLossRate() const;

 private:
  // Bitrate which were used to send packets (actual bitrate could be
  // different).
  DataRate target_sending_rate_;
  // Start time is not included into interval while end time is included.
  Timestamp start_time_;
  TimeDelta duration_;
  // Vectors below updates while receiving feedback.
  std::vector<TimeDelta> received_packets_rtt_;
  std::vector<Timestamp> received_packets_sent_time_;
  std::vector<Timestamp> lost_packets_sent_time_;
  DataSize received_packets_size_;
  DataSize lost_packets_size_;
  bool got_complete_feedback_;
};

class MonitorBlock {
 public:
  // Call this when want to start first MI immideatly.
  MonitorBlock(Timestamp current_time,
               TimeDelta intervals_duration,
               DataRate default_bitrate,
               TimeDelta mi_timeout,
               // Size of this array controls the number of monitor intervals.
               std::vector<DataRate> monitor_intervals_bitrates);
  ~MonitorBlock();
  // MonitorBlock(const MonitorBlock& other);

  // Starts MonitorIntervals when needed. Returns desired sending rate.
  DataRate NotifyCurrentTime(Timestamp time);
  DataRate GetTargetBirtate() const;
  // Returns true if we got complete information about this block.
  bool OnPacketsFeedback(const std::vector<PacketResult>& packets_results);
  void UpdateTimeout(TimeDelta new_timeout);
  bool IsFeedbackCollectingDone() const;
  bool IsTimeoutExpired() const;
  size_t Size() const;

  const MonitorInterval* GetMonitorInterval(size_t index) const;

 private:
  Timestamp last_known_time_;
  std::vector<MonitorInterval> monitor_intervals_;
  std::vector<DataRate> monitor_intervals_bitrates_;
  TimeDelta intervals_duration_;
  // Current estimate of bitrate, called as r in the PCC paper.
  DataRate default_bitrate_;
  TimeDelta mi_timeout_;
  size_t complete_feedback_mi_number_;
  bool got_complete_feedback_;
  RTC_DISALLOW_COPY_AND_ASSIGN(MonitorBlock);
};

}  // namespace pcc
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_PCC_MONITOR_INTERVAL_H_
