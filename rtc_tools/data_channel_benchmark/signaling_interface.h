/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_TOOLS_DATA_CHANNEL_BENCHMARK_SIGNALING_INTERFACE_H_
#define RTC_TOOLS_DATA_CHANNEL_BENCHMARK_SIGNALING_INTERFACE_H_

#include <memory>

#include "api/jsep.h"

namespace webrtc {
class SignalingInterface {
 public:
  virtual ~SignalingInterface() = default;

  virtual void SendIceCandidate(
      const webrtc::IceCandidateInterface* candidate) = 0;
  virtual void SendDescription(
      const webrtc::SessionDescriptionInterface* sdp) = 0;

  virtual void OnRemoteDescription(
      std::function<void(std::unique_ptr<webrtc::SessionDescriptionInterface>
                             sdp)> callback) = 0;
  virtual void OnIceCandidate(
      std::function<void(std::unique_ptr<webrtc::IceCandidateInterface>
                             candidate)> callback) = 0;
};
}  // namespace webrtc

#endif  // RTC_TOOLS_DATA_CHANNEL_BENCHMARK_SIGNALING_INTERFACE_H_
