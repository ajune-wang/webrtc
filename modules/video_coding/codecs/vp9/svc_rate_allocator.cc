/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/svc_rate_allocator.h"

#include <algorithm>
#include <cmath>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {
const float kSpatialLayeringRateScalingFactor = 0.55f;
const float kTemporalLayeringRateScalingFactor = 0.55f;
}  // namespace

SvcRateAllocator::SvcRateAllocator(const VideoCodec& codec) : codec_(codec) {}

BitrateAllocation SvcRateAllocator::GetAllocation(uint32_t total_bitrate_bps,
                                                  uint32_t framerate_fps) {
  BitrateAllocation bitrate_allocator;

  size_t num_spatial_layers = codec_.VP9().numberOfSpatialLayers;
  RTC_CHECK(num_spatial_layers > 0);
  size_t num_temporal_layers = codec_.VP9().numberOfTemporalLayers;
  RTC_CHECK(num_temporal_layers > 0);

  // If there is no enough bitrate to keep quality of all spatial layers on
  // minimal acceptable level then enhancement layers are disabled one by one
  // (layer bitrate is set to zero) until that condition is met.
  for (; num_spatial_layers > 1; --num_spatial_layers) {
    std::vector<size_t> spatial_layer_bitrate_bps =
        SplitBitrate(num_spatial_layers, total_bitrate_bps,
                     kSpatialLayeringRateScalingFactor);

    bool all_layers_got_enough_bitrate = true;

    size_t excess_rate = 0;
    for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
      RTC_DCHECK_GT(codec_.spatialLayers[sl_idx].minBitrate, 0);
      RTC_DCHECK_GT(codec_.spatialLayers[sl_idx].maxBitrate, 0);

      const size_t min_bitrate_bps =
          codec_.spatialLayers[sl_idx].minBitrate * 1000;
      const size_t max_bitrate_bps =
          codec_.spatialLayers[sl_idx].maxBitrate * 1000;

      spatial_layer_bitrate_bps[sl_idx] += excess_rate;
      if (spatial_layer_bitrate_bps[sl_idx] < max_bitrate_bps) {
        excess_rate = 0;
      } else {
        excess_rate = spatial_layer_bitrate_bps[sl_idx] - max_bitrate_bps;
      }

      if (spatial_layer_bitrate_bps[sl_idx] > max_bitrate_bps) {
        spatial_layer_bitrate_bps[sl_idx] = max_bitrate_bps;
      }

      if (spatial_layer_bitrate_bps[sl_idx] < min_bitrate_bps) {
        all_layers_got_enough_bitrate = false;
      }
    }

    if (all_layers_got_enough_bitrate) {
      break;
    }
  }

  std::vector<size_t> spatial_layer_bitrate_bps = SplitBitrate(
      num_spatial_layers, total_bitrate_bps, kSpatialLayeringRateScalingFactor);

  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    std::vector<size_t> temporal_layer_bitrate_bps =
        SplitBitrate(num_temporal_layers, spatial_layer_bitrate_bps[sl_idx],
                     kTemporalLayeringRateScalingFactor);

    for (size_t tl_idx = 0; tl_idx < num_temporal_layers; ++tl_idx) {
      bitrate_allocator.SetBitrate(sl_idx, num_temporal_layers - tl_idx - 1,
                                   temporal_layer_bitrate_bps[tl_idx]);
    }
  }

  return bitrate_allocator;
}

uint32_t SvcRateAllocator::GetPreferredBitrateBps(uint32_t framerate) {
  // Create a temporary instance without temporal layers, as they may be
  // stateful, and updating the bitrate to max here can cause side effects.
  SvcRateAllocator temp_allocator(codec_);
  BitrateAllocation allocation =
      temp_allocator.GetAllocation(codec_.maxBitrate * 1000, framerate);
  return allocation.get_sum_bps();
}

std::vector<size_t> SvcRateAllocator::SplitBitrate(size_t num_layers,
                                                   size_t total_bitrate,
                                                   float rate_scaling_factor) {
  std::vector<size_t> bitrates;

  float denominator_srf = 0.0f;
  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    denominator_srf += std::pow(rate_scaling_factor, layer_idx);
  }

  float numerator_srf = std::pow(rate_scaling_factor, num_layers - 1);
  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    bitrates.push_back(numerator_srf * total_bitrate / denominator_srf);
    numerator_srf /= rate_scaling_factor;
  }

  return bitrates;
}

}  // namespace webrtc
