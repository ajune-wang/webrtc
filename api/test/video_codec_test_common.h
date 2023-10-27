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
#include <string>
#include <vector>

#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/video/resolution.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"

namespace webrtc {
namespace test {

// Encoding settings shared by `VideoCodecTester` and `VideoCodecStats`.
struct EncodingSettings {
  SdpVideoFormat sdp_video_format = SdpVideoFormat("VP8");
  ScalabilityMode scalability_mode = ScalabilityMode::kL1T1;

  struct LayerId {
    int spatial_idx = 0;
    int temporal_idx = 0;

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

// A helper function that creates map of RTP timestamp to `EncodingSettings`. If
// size of `layer_bitrates_kbps` is one the value is interpreted as the total
// bitrate and, in the case if `scalability_mode` implies multiple layers, is
// distributed between the layers by means of the default codec type specific
// bitrate allocators. Otherwise, the size of `layer_bitrates_kbps` should be
// equal to the total number of layers indicated by `scalability_mode'.
std::map<uint32_t, EncodingSettings> CreateEncodingSettings(
    std::string codec_type,
    std::string scalability_name,
    int width,
    int height,
    std::vector<int> layer_bitrates_kbps,
    double framerate_fps,
    int num_frames,
    uint32_t initial_timestamp_rtp = 90000);

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_TEST_COMMON_H_
