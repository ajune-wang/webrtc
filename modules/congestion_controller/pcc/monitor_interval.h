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
  // Notice, complete information about packets is received only when
  // received the feedback about the first packet which were sent after the
  // end of the monitor interval.
  void OnPacketsFeedback(const std::vector<PacketResult>& packets_results);

  bool IsFeedbackCollectionDone() const;
  Timestamp GetEndTime() const;

  double GetLossRate() const;
  double ComputeRttGradient(double rtt_gradient_threshold) const;
  DataRate GetTargetSendingRate() const;
  const std::vector<TimeDelta>& GetReceivedPacketsRtt() const;
  const std::vector<Timestamp>& GetReceivedPacketsSentTime() const;
  const std::vector<Timestamp>& GetLostPacketsSentTime() const;

 private:
  // Target bitrate used to generate and pace the outgoing packets.
  // Actually sent bitrate might not match the target exactly.
  DataRate target_sending_rate_;
  // Start time is not included into interval while end time is included.
  Timestamp start_time_;
  TimeDelta interval_duration_;
  // Vectors below updates while receiving feedback.
  std::vector<TimeDelta> received_packets_rtt_;
  std::vector<Timestamp> received_packets_sent_time_;
  std::vector<Timestamp> lost_packets_sent_time_;
  DataSize received_packets_size_;
  DataSize lost_packets_size_;
  bool feedback_collection_done_;
};

}  // namespace pcc
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_PCC_MONITOR_INTERVAL_H_
