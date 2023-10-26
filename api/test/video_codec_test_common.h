/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEO_CODEC_TEST_COMMON_H_
#define API_TEST_VIDEO_CODEC_TEST_COMMON_H_

#include <map>

#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/video/resolution.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"

namespace webrtc {
namespace test {

// Encoding settings shared by `VideoCodecTester` and `VideoCodecStats`.
struct EncodingSettings {
  SdpVideoFormat sdp_video_format;
  ScalabilityMode scalability_mode;

  struct LayerId {
    int spatial_idx;
    int temporal_idx;

    bool operator==(const LayerId& o) const {
      return spatial_idx == o.spatial_idx && temporal_idx == o.temporal_idx;
    }

    bool operator<(const LayerId& o) const {
      if (spatial_idx < o.spatial_idx)
        return true;
      if (spatial_idx == o.spatial_idx && temporal_idx < o.temporal_idx)
        return true;
      return false;
    }
  };

  struct LayerSettings {
    Resolution resolution;
    Frequency framerate;
    DataRate bitrate;
  };

  std::map<LayerId, LayerSettings> layers_settings;

  // Returns target bitrate for given layer. If `layer_id` is not specified,
  // returned value is a sum of bitrates of all layers in `layers_settings`.
  DataRate GetTargetBitrate(
      absl::optional<LayerId> layer_id = absl::nullopt) const;

  // Returns target frame rate for given layer. If `layer_id` is not
  // specified, returned value is a frame rate of the highest layer in
  // `layers_settings`.
  Frequency GetTargetFramerate(
      absl::optional<LayerId> layer_id = absl::nullopt) const;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_TEST_COMMON_H_
