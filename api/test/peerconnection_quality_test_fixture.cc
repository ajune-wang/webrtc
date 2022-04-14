/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/peerconnection_quality_test_fixture.h"

#include "absl/types/optional.h"
#include "api/array_view.h"

namespace webrtc {
namespace webrtc_pc_e2e {

using VideoCodecConfig = ::webrtc::webrtc_pc_e2e::
    PeerConnectionE2EQualityTestFixture::VideoCodecConfig;
using VideoSubscription = ::webrtc::webrtc_pc_e2e::
    PeerConnectionE2EQualityTestFixture::VideoSubscription;

absl::optional<VideoSubscription::Resolution>
PeerConnectionE2EQualityTestFixture::VideoSubscription::GetMaximumResolution(
    rtc::ArrayView<const VideoConfig> video_configs) {
  if (video_configs.empty()) {
    return absl::nullopt;
  }

  VideoSubscription::Resolution max_resolution;
  for (const VideoConfig& config : video_configs) {
    if (max_resolution.width < config.width) {
      max_resolution.width = config.width;
    }
    if (max_resolution.height < config.height) {
      max_resolution.height = config.height;
    }
    if (max_resolution.fps < config.fps) {
      max_resolution.fps = config.fps;
    }
  }
  return max_resolution;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
