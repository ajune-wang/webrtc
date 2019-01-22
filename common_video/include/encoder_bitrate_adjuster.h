/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INCLUDE_ENCODER_BITRATE_ADJUSTER_H_
#define COMMON_VIDEO_INCLUDE_ENCODER_BITRATE_ADJUSTER_H_

#include <memory>

#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/include/encoder_overshoot_detector.h"

namespace webrtc {

class EncoderBitrateAdjuster {
 public:
  // Size of sliding window used to track overshoot rate.
  static constexpr int64_t kWindowSizeMs = 6000;
  // Minimum number of frames since last layout change required to trust the
  // overshoot statistics. Otherwise falls back to default utilization.
  static constexpr size_t kMinFramesSinceLayoutChange = 30;
  // Default utilization, before reliable metrics are available, is set to 20%
  // overshoot. This is conservative so that badly misbehaving encoders don't
  // build too much queue at the very start.
  static constexpr double kDefaultUtilizationFactor = 1.2;

  explicit EncoderBitrateAdjuster(const VideoCodec& codec_settings);
  ~EncoderBitrateAdjuster();

  VideoBitrateAllocation OnRateAllocation(
      const VideoBitrateAllocation& bitrate_allocation,
      int framerate_fps);
  void OnEncoderInfo(const VideoEncoder::EncoderInfo& encoder_info);
  void OnEncodedImage(size_t size_bytes, int spatial_index, int temporal_index);

 private:
  VideoBitrateAllocation current_bitrate_allocation_;
  int current_total_framerate_fps_;
  absl::InlinedVector<uint8_t, kMaxTemporalStreams>
      current_fps_allocation_[kMaxSpatialLayers];

  size_t frames_since_layout_change_;
  std::unique_ptr<EncoderOvershootDetector>
      overshoot_detectors_[kMaxSpatialLayers][kMaxTemporalStreams];

  // Minimum bitrates allowed, per spatial layer.
  uint32_t min_bitrates_bps_[kMaxSpatialLayers];
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INCLUDE_ENCODER_BITRATE_ADJUSTER_H_
