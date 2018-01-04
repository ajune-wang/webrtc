/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_PACER_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_PACER_CONTROLLER_H_

#include <memory>

#include "modules/pacing/paced_sender.h"
#include "network_control/include/network_types.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
class Clock;
namespace network {

// Wrapper class to control pacer using task queues
class PacerController {
 public:
  PacerController(const Clock* clock, PacedSender* pacer);
  ~PacerController();
  bool GetPacerConfigured();

 private:
  // Add comment
  class Impl;
  std::unique_ptr<Impl> impl_;

 public:
  network::CongestionWindow::MessageHandler CongestionWindowReceiver;
  network::NetworkAvailability::MessageHandler NetworkAvailabilityReceiver;
  network::NetworkRouteChange::MessageHandler NetworkRouteChangeReceiver;
  network::OutstandingData::MessageHandler OutstandingDataReceiver;
  network::PacerConfig::MessageHandler PacerConfigReceiver;
  network::ProbeClusterConfig::MessageHandler ProbeClusterConfigReceiver;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PacerController);
};
}  // namespace network
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_PACER_CONTROLLER_H_
