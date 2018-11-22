/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_RTC_BASE_EXPERIMENTS_CONGESTION_CONTROLLER_EXPERIMENT_H_
#define WEBRTC_RTC_BASE_EXPERIMENTS_CONGESTION_CONTROLLER_EXPERIMENT_H_

namespace webrtc {
class CongestionControllerExperiment {
 public:
  static bool BbrControllerEnabled();
  static bool InjectedControllerEnabled();
};

}  // namespace webrtc

#endif  // WEBRTC_RTC_BASE_EXPERIMENTS_CONGESTION_CONTROLLER_EXPERIMENT_H_
