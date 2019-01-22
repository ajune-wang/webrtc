/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/include/encoder_bitrate_adjuster.h"

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

constexpr int64_t EncoderBitrateAdjuster::kWindowSizeMs;
constexpr size_t EncoderBitrateAdjuster::kMinFramesSinceLayoutChange;
constexpr double EncoderBitrateAdjuster::kDefaultUtilizationFactor;

EncoderBitrateAdjuster::EncoderBitrateAdjuster(const VideoCodec& codec_settings)
    : current_total_framerate_fps_(0),
      frames_since_layout_change_(0),
      min_bitrates_bps_{} {
  if (codec_settings.codecType == VideoCodecType::kVideoCodecVP8 ||
      codec_settings.codecType == VideoCodecType::kVideoCodecH264) {
    for (size_t si = 0; si < codec_settings.numberOfSimulcastStreams; ++si) {
      if (codec_settings.simulcastStream[si].active) {
        min_bitrates_bps_[si] =
            std::max(codec_settings.minBitrate,
                     codec_settings.simulcastStream[si].minBitrate * 1000);
      }
    }
  } else if (codec_settings.codecType == VideoCodecType::kVideoCodecVP9) {
    for (size_t si = 0; si < codec_settings.VP9().numberOfSpatialLayers; ++si) {
      if (codec_settings.spatialLayers[si].active) {
        min_bitrates_bps_[si] =
            std::max(codec_settings.minBitrate,
                     codec_settings.spatialLayers[si].minBitrate * 1000);
      }
    }
  }
}

EncoderBitrateAdjuster::~EncoderBitrateAdjuster() = default;

VideoBitrateAllocation EncoderBitrateAdjuster::OnRateAllocation(
    const VideoBitrateAllocation& bitrate_allocation,
    int framerate_fps) {
  const int64_t now_ms = rtc::TimeMillis();

  // Store, per spatial layer, how many active spatial layers we have.
  size_t active_tls_[kMaxSpatialLayers] = {};

  // First update all overshoot detectors with correct rates.
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    active_tls_[si] = 0;
    for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
      // Layer is enabled iff it has both positive bitrate and framerate target.
      if (bitrate_allocation.GetBitrate(si, ti) > 0 &&
          current_fps_allocation_[si].size() > ti &&
          current_fps_allocation_[si][ti] > 0) {
        ++active_tls_[si];
        if (!overshoot_detectors_[si][ti]) {
          overshoot_detectors_[si][ti] =
              absl::make_unique<EncoderOvershootDetector>(kWindowSizeMs);
          frames_since_layout_change_ = 0;
        }
      } else if (overshoot_detectors_[si][ti]) {
        // Layer removed, destroy overshoot detector.
        overshoot_detectors_[si][ti].reset();
        frames_since_layout_change_ = 0;
      }
    }
  }

  // Next updated detectors and populate the adjusted allocation.
  VideoBitrateAllocation adjusted_allocation;
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    const uint32_t spatial_layer_bitrate_bps =
        bitrate_allocation.GetSpatialLayerSum(si);

    // Adjustment is done per spatial layer only (not per temporal layer).
    double utilization_factor;
    if (active_tls_[si] == 0 && spatial_layer_bitrate_bps > 0) {
      // No signaled temporal layers, but bitrate allocation indicates usage/
      // This indicates bitrate dynamic mode; pass bitrate through without any
      // change.
      utilization_factor = 1.0;
    } else if (active_tls_[si] == 1) {
      // A single active temporal layer, this might mean single layer or that
      // encoder does not supporting temporal layers. Merge target bitrates for
      // this spatial layer.
      RTC_DCHECK(overshoot_detectors_[si][0]);
      overshoot_detectors_[si][0]->SetTargetRate(
          DataRate::bps(spatial_layer_bitrate_bps), framerate_fps, now_ms);
      if (frames_since_layout_change_ < kMinFramesSinceLayoutChange) {
        utilization_factor = kDefaultUtilizationFactor;
      } else {
        utilization_factor =
            overshoot_detectors_[si][0]->GetUtilizationFactor(now_ms).value_or(
                kDefaultUtilizationFactor);
      }
    } else if (spatial_layer_bitrate_bps > 0) {
      double utilization_factor_sum = 0.0;
      bool use_weighted_sum =
          frames_since_layout_change_ >= kMinFramesSinceLayoutChange;
      // Multiple temporal layers enabled for this spatial layer. Update rate
      // for each of them and make a weighted average of utilization factors,
      // with bitrate fraction used as weight.
      for (size_t ti = 0; ti < active_tls_[si]; ++ti) {
        RTC_DCHECK(overshoot_detectors_[si][ti]);
        const double fps_fraction =
            static_cast<double>(current_fps_allocation_[si][ti]) /
            VideoEncoder::EncoderInfo::kMaxFramerateFraction;
        overshoot_detectors_[si][ti]->SetTargetRate(
            DataRate::bps(bitrate_allocation.GetBitrate(si, ti)),
            static_cast<int>(fps_fraction * current_total_framerate_fps_ + 0.5),
            now_ms);
        const absl::optional<double> utilization_factor =
            overshoot_detectors_[si][ti]->GetUtilizationFactor(now_ms);
        if (utilization_factor) {
          const double weight =
              static_cast<double>(bitrate_allocation.GetBitrate(si, ti)) /
              spatial_layer_bitrate_bps;
          utilization_factor_sum += weight * utilization_factor.value();
        } else {
          // No stats available for this layer, use default for this spatial
          // layer.
          use_weighted_sum = false;
        }
      }

      if (use_weighted_sum) {
        utilization_factor = utilization_factor_sum;
      } else {
        utilization_factor = kDefaultUtilizationFactor;
      }
    }

    // Don't boost target bitrate if encoder is under-using.
    utilization_factor = std::max(utilization_factor, 1.0);

    // Don't reduce encoder target below 50%, in which case the frame dropper
    // should kick in instead.
    utilization_factor = std::min(utilization_factor, 2.0);

    if (min_bitrates_bps_[si] > 0 && spatial_layer_bitrate_bps > 0 &&
        min_bitrates_bps_[si] < spatial_layer_bitrate_bps) {
      // Make sure rate adjuster don't push target bitrate below minimum.
      utilization_factor = std::min(
          utilization_factor, static_cast<double>(spatial_layer_bitrate_bps) /
                                  min_bitrates_bps_[si]);
    }

    // Finally populate the adjusted allocation with determined utilization
    // factor.
    for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
      if (bitrate_allocation.HasBitrate(si, ti)) {
        adjusted_allocation.SetBitrate(
            si, ti,
            static_cast<uint32_t>(bitrate_allocation.GetBitrate(si, ti) /
                                      utilization_factor +
                                  0.5));
      }
    }

    // In case of rounding errors, add bitrate to TL0 until min bitrate
    // constraint has been met.
    uint32_t adjusted_spatial_layer_sum =
        adjusted_allocation.GetSpatialLayerSum(si);
    if (adjusted_spatial_layer_sum < min_bitrates_bps_[si]) {
      adjusted_allocation.SetBitrate(si, 0,
                                     adjusted_allocation.GetBitrate(si, 0) +
                                         min_bitrates_bps_[si] -
                                         adjusted_spatial_layer_sum);
    }
  }

  current_bitrate_allocation_ = bitrate_allocation;
  current_total_framerate_fps_ = framerate_fps;

  return adjusted_allocation;
}

void EncoderBitrateAdjuster::OnEncoderInfo(
    const VideoEncoder::EncoderInfo& encoder_info) {
  // Copy allocation into current state and re-allocate.
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    current_fps_allocation_[si] = encoder_info.fps_allocation[si];
  }

  // Trigger re-allocation so that overshoot detectors have correct targets.
  OnRateAllocation(current_bitrate_allocation_, current_total_framerate_fps_);
}

void EncoderBitrateAdjuster::OnEncodedImage(size_t size_bytes,
                                            int spatial_index,
                                            int temporal_index) {
  ++frames_since_layout_change_;
  // Detectors may not exist, for instance if ScreenshareLayers is used.
  auto& detector = overshoot_detectors_[spatial_index][temporal_index];
  if (detector) {
    detector->OnEncodedFrame(size_bytes, rtc::TimeMillis());
  }
}

}  // namespace webrtc
