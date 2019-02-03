/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/encoder_bitrate_adjuster.h"

#include <vector>

#include "absl/memory/memory.h"
#include "api/units/data_rate.h"
#include "rtc_base/fake_clock.h"
#include "test/gtest.h"

namespace webrtc {

class EncoderBitrateAdjusterTest : public ::testing::Test {
 public:
  static constexpr int64_t kWindowSizeMs = 1000;
  static constexpr int kDefaultBitrateBps = 300000;
  static constexpr int kDefaultFrameRateFps = 30;
  EncoderBitrateAdjusterTest()
      : current_framerate_fps_(0),
        target_bitrate_(DataRate::bps(kDefaultBitrateBps)),
        target_framerate_fps_(kDefaultFrameRateFps),
        fps_fraction_counters_{{}} {}

 protected:
  void SetUpAdjuster(size_t num_spatial_layers,
                     size_t num_temporal_layers,
                     bool vp9_svc) {
    // Initialize some default VideoCodec instance with the given number of
    // layers.
    if (vp9_svc) {
      codec_.codecType = VideoCodecType::kVideoCodecVP9;
      codec_.numberOfSimulcastStreams = 1;
      codec_.VP9()->numberOfSpatialLayers = num_spatial_layers;
      codec_.VP9()->numberOfTemporalLayers = num_temporal_layers;
      for (size_t si = 0; si < num_spatial_layers; ++si) {
        codec_.spatialLayers[si].minBitrate = 100 * (si << si);
        codec_.spatialLayers[si].targetBitrate = 200 * (si << si);
        codec_.spatialLayers[si].maxBitrate = 300 * (si << si);
        codec_.spatialLayers[si].active = true;
        codec_.spatialLayers[si].numberOfTemporalLayers = num_temporal_layers;
      }
    } else {
      codec_.codecType = VideoCodecType::kVideoCodecVP8;
      codec_.numberOfSimulcastStreams = num_spatial_layers;
      codec_.VP8()->numberOfTemporalLayers = num_temporal_layers;
      for (size_t si = 0; si < num_spatial_layers; ++si) {
        codec_.simulcastStream[si].minBitrate = 100 * (si << si);
        codec_.simulcastStream[si].targetBitrate = 200 * (si << si);
        codec_.simulcastStream[si].maxBitrate = 300 * (si << si);
        codec_.simulcastStream[si].active = true;
        codec_.simulcastStream[si].numberOfTemporalLayers = num_temporal_layers;
      }
    }

    for (size_t si = 0; si < num_spatial_layers; ++si) {
      for (size_t ti = 0; ti < num_temporal_layers; ++ti) {
        encoder_info_.fps_allocation[si][ti] =
            VideoEncoder::EncoderInfo::kMaxFramerateFraction >>
            (num_temporal_layers - ti - 1);
      }
    }

    adjuster_ = absl::make_unique<EncoderBitrateAdjuster>(codec_);
    adjuster_->OnEncoderInfo(encoder_info_);
    current_adjusted_allocation_ = adjuster_->AdjustRateAllocation(
        current_input_allocation_, current_framerate_fps_);
  }

  void InsertFrames(std::vector<double> utilization_factors,
                    int64_t duration_ms) {
    size_t kMaxFrameSize = 100000;
    uint8_t buffer[kMaxFrameSize];

    const int64_t start_us = rtc::TimeMicros();
    while (rtc::TimeMicros() <
           start_us + (duration_ms * rtc::kNumMicrosecsPerMillisec)) {
      clock_.AdvanceTimeMicros(rtc::kNumMicrosecsPerSec /
                               target_framerate_fps_);
      for (size_t si = 0; si < NumSpatialLayers(); ++si) {
        for (size_t ti = 0; ti < NumTemporalLayers(si); ++ti) {
          fps_fraction_counters_[si][ti] +=
              encoder_info_.fps_allocation[si][ti];
          if (fps_fraction_counters_[si][ti] >=
              VideoEncoder::EncoderInfo::kMaxFramerateFraction) {
            fps_fraction_counters_[si][ti] -=
                VideoEncoder::EncoderInfo::kMaxFramerateFraction;
            uint32_t layer_bitrate_bps =
                current_adjusted_allocation_.GetBitrate(si, ti);
            double layer_framerate_fps = current_framerate_fps_;
            if (encoder_info_.fps_allocation[si].size() > ti) {
              layer_framerate_fps =
                  (layer_framerate_fps * encoder_info_.fps_allocation[si][ti]) /
                  VideoEncoder::EncoderInfo::kMaxFramerateFraction;
            }
            double utilization_factor = 1.0;
            if (utilization_factors.size() > si) {
              utilization_factor = utilization_factors[si];
            }
            size_t frame_size_bytes = utilization_factor *
                                      (layer_bitrate_bps / 8.0) /
                                      layer_framerate_fps;

            EncodedImage image(buffer, 0, kMaxFrameSize);
            image.set_size(frame_size_bytes);
            image.SetSpatialIndex(si);
            adjuster_->OnEncodedFrame(image, ti);
            // Only on temporal layers per spatial one, continue to next spatial
            // index.
            continue;
          }
        }
      }
    }
  }

  size_t NumSpatialLayers() const {
    if (codec_.codecType == VideoCodecType::kVideoCodecVP9) {
      return codec_.VP9().numberOfSpatialLayers;
    }
    return codec_.numberOfSimulcastStreams;
  }

  size_t NumTemporalLayers(int spatial_index) {
    if (codec_.codecType == VideoCodecType::kVideoCodecVP9) {
      return codec_.spatialLayers[spatial_index].numberOfTemporalLayers;
    }
    return codec_.simulcastStream[spatial_index].numberOfTemporalLayers;
  }

  VideoCodec codec_;
  VideoEncoder::EncoderInfo encoder_info_;
  std::unique_ptr<EncoderBitrateAdjuster> adjuster_;
  VideoBitrateAllocation current_input_allocation_;
  VideoBitrateAllocation current_adjusted_allocation_;
  int current_framerate_fps_;
  rtc::ScopedFakeClock clock_;
  DataRate target_bitrate_;
  int target_framerate_fps_;

  int fps_fraction_counters_[kMaxSpatialLayers][kMaxTemporalStreams];
};

TEST_F(EncoderBitrateAdjusterTest, SingleLayer) {
  // Single layer, well behaved encoder.
  current_input_allocation_.SetBitrate(0, 0, 300000);
  current_framerate_fps_ = 30;
  SetUpAdjuster(1, 1, false);
  InsertFrames({1.0}, kWindowSizeMs);
  current_adjusted_allocation_ = adjuster_->AdjustRateAllocation(
      current_input_allocation_, current_framerate_fps_);
  // Adjusted allocation near input. Allow 1% error margin due to rounding
  // errors etc.
  EXPECT_NEAR(current_input_allocation_.get_sum_bps(),
              current_adjusted_allocation_.get_sum_bps(),
              current_input_allocation_.get_sum_bps() / 100);
}

}  // namespace webrtc
