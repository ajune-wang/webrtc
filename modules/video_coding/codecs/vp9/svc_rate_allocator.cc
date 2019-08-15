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
#include <cstddef>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {

const float kSpatialLayeringRateScalingFactor = 0.55f;
const float kTemporalLayeringRateScalingFactor = 0.55f;

static size_t GetNumActiveSpatialLayers(const VideoCodec& codec) {
  RTC_DCHECK_EQ(codec.codecType, kVideoCodecVP9);
  RTC_DCHECK_GT(codec.VP9().numberOfSpatialLayers, 0u);

  size_t num_spatial_layers = 0;
  for (; num_spatial_layers < codec.VP9().numberOfSpatialLayers;
       ++num_spatial_layers) {
    if (!codec.spatialLayers[num_spatial_layers].active) {
      // TODO(bugs.webrtc.org/9350): Deactivation of middle layer is not
      // implemented. For now deactivation of a VP9 layer deactivates all
      // layers above the deactivated one.
      break;
    }
  }

  return num_spatial_layers;
}

absl::optional<std::vector<DataRate>> AdjustAndVerify(
    const VideoCodec& codec,
    const std::vector<DataRate>& spatial_layer_rates) {
  std::vector<DataRate> adjusted_spatial_layer_rates;
  // Keep track of rate that couldn't be applied to the previous layer due to
  // max bitrate constraint, try to pass it forward to the next one.
  DataRate excess_rate = DataRate::Zero();
  for (size_t sl_idx = 0; sl_idx < spatial_layer_rates.size(); ++sl_idx) {
    DataRate min_rate = DataRate::kbps(codec.spatialLayers[sl_idx].minBitrate);
    DataRate max_rate = DataRate::kbps(codec.spatialLayers[sl_idx].maxBitrate);

    DataRate layer_rate = spatial_layer_rates[sl_idx] + excess_rate;
    if (layer_rate < min_rate) {
      // Not enough rate to reach min bitrate for desired number of layers,
      // abort allocation.
      return absl::nullopt;
    }

    if (layer_rate <= max_rate) {
      excess_rate = DataRate::Zero();
      adjusted_spatial_layer_rates.push_back(layer_rate);
    } else {
      excess_rate = layer_rate - max_rate;
      adjusted_spatial_layer_rates.push_back(max_rate);
    }
  }

  return adjusted_spatial_layer_rates;
}

static std::vector<DataRate> SplitBitrate(size_t num_layers,
                                          DataRate total_bitrate,
                                          float rate_scaling_factor) {
  std::vector<DataRate> bitrates;

  double denominator = 0.0;
  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    denominator += std::pow(rate_scaling_factor, layer_idx);
  }

  double numerator = std::pow(rate_scaling_factor, num_layers - 1);
  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    bitrates.push_back(numerator * total_bitrate / denominator);
    numerator /= rate_scaling_factor;
  }

  const DataRate sum =
      std::accumulate(bitrates.begin(), bitrates.end(), DataRate::Zero());

  // Keep the sum of split bitrates equal to the total bitrate by adding or
  // subtracting bits, which were lost due to rounding, to the latest layer.
  if (total_bitrate > sum) {
    bitrates.back() += total_bitrate - sum;
  } else if (total_bitrate < sum) {
    bitrates.back() -= sum - total_bitrate;
  }

  return bitrates;
}

SvcRateAllocator::SvcRateAllocator(const VideoCodec& codec) : codec_(codec) {
  RTC_DCHECK_EQ(codec.codecType, kVideoCodecVP9);
  RTC_DCHECK_GT(codec.VP9().numberOfSpatialLayers, 0u);
  RTC_DCHECK_GT(codec.VP9().numberOfTemporalLayers, 0u);
  for (size_t layer_idx = 0; layer_idx < codec.VP9().numberOfSpatialLayers;
       ++layer_idx) {
    // Verify min <= target <= max.
    if (codec.spatialLayers[layer_idx].active) {
      RTC_DCHECK_GT(codec.spatialLayers[layer_idx].maxBitrate, 0);
      RTC_DCHECK_GE(codec.spatialLayers[layer_idx].maxBitrate,
                    codec.spatialLayers[layer_idx].minBitrate);
      RTC_DCHECK_GE(codec.spatialLayers[layer_idx].targetBitrate,
                    codec.spatialLayers[layer_idx].minBitrate);
      RTC_DCHECK_GE(codec.spatialLayers[layer_idx].maxBitrate,
                    codec.spatialLayers[layer_idx].targetBitrate);
    }
  }
}

VideoBitrateAllocation SvcRateAllocator::Allocate(
    VideoBitrateAllocationParameters parameters) {
  DataRate total_bitrate = parameters.total_bitrate;
  if (codec_.maxBitrate != 0) {
    total_bitrate = std::min(total_bitrate, DataRate::kbps(codec_.maxBitrate));
  }

  if (codec_.spatialLayers[0].targetBitrate == 0) {
    // Delegate rate distribution to VP9 encoder wrapper if bitrate thresholds
    // are not set.
    VideoBitrateAllocation bitrate_allocation;
    bitrate_allocation.SetBitrate(0, 0, total_bitrate.bps());
    return bitrate_allocation;
  }

  size_t num_spatial_layers = GetNumActiveSpatialLayers(codec_);
  if (num_spatial_layers == 0) {
    return VideoBitrateAllocation();  // All layers are deactivated.
  }

  DataRate stable_rate = parameters.stable_bitrate > DataRate::Zero()
                             ? parameters.stable_bitrate
                             : total_bitrate;
  if (codec_.mode == VideoCodecMode::kRealtimeVideo) {
    return GetAllocationNormalVideo(total_bitrate, stable_rate,
                                    num_spatial_layers);
  } else {
    return GetAllocationScreenSharing(total_bitrate, stable_rate,
                                      num_spatial_layers);
  }
}

VideoBitrateAllocation SvcRateAllocator::GetAllocationNormalVideo(
    DataRate total_bitrate,
    DataRate stable_bitrate,
    size_t num_spatial_layers) const {
  // Distribute total bitrate across spatial layers. If there is not enough
  // bitrate to provide all layers with at least minimum required bitrate
  // then number of layers is reduced by one and distribution is repeated
  // until that condition is met or if number of layers is reduced to one.
  // Use the stable bitrate to determine number of layers to use, but the total
  // bitrate when allocating across those layers.
  for (; num_spatial_layers > 0; --num_spatial_layers) {
    std::vector<DataRate> split_rates = SplitBitrate(
        num_spatial_layers, stable_bitrate, kSpatialLayeringRateScalingFactor);
    if (AdjustAndVerify(codec_, split_rates)) {
      break;
    }
  }

  std::vector<DataRate> spatial_layer_rates;
  if (num_spatial_layers == 0) {
    // Not enough rate for even the base layer. Force allocation at the total
    // bitrate anyway.
    num_spatial_layers = 1;
    spatial_layer_rates.push_back(total_bitrate);
  } else {
    spatial_layer_rates = *AdjustAndVerify(
        codec_, SplitBitrate(num_spatial_layers, total_bitrate,
                             kSpatialLayeringRateScalingFactor));
  }

  VideoBitrateAllocation bitrate_allocation;

  const size_t num_temporal_layers = codec_.VP9().numberOfTemporalLayers;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    std::vector<DataRate> temporal_layer_rates =
        SplitBitrate(num_temporal_layers, spatial_layer_rates[sl_idx],
                     kTemporalLayeringRateScalingFactor);

    // Distribute rate across temporal layers. Allocate more bits to lower
    // layers since they are used for prediction of higher layers and their
    // references are far apart.
    if (num_temporal_layers == 1) {
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_rates[0].bps());
    } else if (num_temporal_layers == 2) {
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_rates[1].bps());
      bitrate_allocation.SetBitrate(sl_idx, 1, temporal_layer_rates[0].bps());
    } else {
      RTC_CHECK_EQ(num_temporal_layers, 3);
      // In case of three temporal layers the high layer has two frames and the
      // middle layer has one frame within GOP (in between two consecutive low
      // layer frames). Thus high layer requires more bits (comparing pure
      // bitrate of layer, excluding bitrate of base layers) to keep quality on
      // par with lower layers.
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_rates[2].bps());
      bitrate_allocation.SetBitrate(sl_idx, 1, temporal_layer_rates[0].bps());
      bitrate_allocation.SetBitrate(sl_idx, 2, temporal_layer_rates[1].bps());
    }
  }

  return bitrate_allocation;
}

// Bit-rate is allocated in such a way, that the highest enabled layer will have
// between min and max bitrate, and all others will have exactly target
// bit-rate allocated.
VideoBitrateAllocation SvcRateAllocator::GetAllocationScreenSharing(
    DataRate total_bitrate,
    DataRate stable_bitrate,
    size_t num_spatial_layers) const {
  if (num_spatial_layers == 0 ||
      total_bitrate < DataRate::kbps(codec_.spatialLayers[0].minBitrate)) {
    return VideoBitrateAllocation();
  }
  VideoBitrateAllocation bitrate_allocation;

  DataRate allocated_rate = DataRate::Zero();
  DataRate top_layer_rate = DataRate::Zero();
  size_t sl_idx;
  for (sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    const DataRate min_rate =
        DataRate::kbps(codec_.spatialLayers[sl_idx].minBitrate);
    const DataRate target_rate =
        DataRate::kbps(codec_.spatialLayers[sl_idx].targetBitrate);

    if (allocated_rate + min_rate > stable_bitrate) {
      // Use stable rate to determine if layer should be enabled.
      break;
    }

    top_layer_rate = std::min(target_rate, total_bitrate - allocated_rate);
    bitrate_allocation.SetBitrate(sl_idx, 0, top_layer_rate.bps());
    allocated_rate += top_layer_rate;
  }

  RTC_DCHECK_GT(sl_idx, 0);
  if (total_bitrate - allocated_rate > DataRate::Zero()) {
    // Add leftover to the last allocated layer.
    const DataRate max_rate =
        DataRate::kbps(codec_.spatialLayers[sl_idx].minBitrate);
    top_layer_rate += total_bitrate - allocated_rate;
    bitrate_allocation.SetBitrate(sl_idx - 1, 0,
                                  std::min(top_layer_rate, max_rate).bps());
  }

  return bitrate_allocation;
}

DataRate SvcRateAllocator::GetMaxBitrate(const VideoCodec& codec) {
  const size_t num_spatial_layers = GetNumActiveSpatialLayers(codec);

  DataRate max_bitrate = DataRate::Zero();
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    max_bitrate += DataRate::kbps(codec.spatialLayers[sl_idx].maxBitrate);
  }

  if (codec.maxBitrate != 0) {
    max_bitrate = std::min(max_bitrate, DataRate::kbps(codec.maxBitrate));
  }

  return max_bitrate;
}

DataRate SvcRateAllocator::GetPaddingBitrate(const VideoCodec& codec) {
  const size_t num_spatial_layers = GetNumActiveSpatialLayers(codec);
  if (num_spatial_layers == 0) {
    return DataRate::Zero();  // All layers are deactivated.
  }

  if (codec.mode == VideoCodecMode::kRealtimeVideo) {
    float scale_factor = 0.0;
    for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
      scale_factor += std::pow(kSpatialLayeringRateScalingFactor, sl_idx);
    }
    DataRate min_bitrate =
        DataRate::kbps(codec.spatialLayers[num_spatial_layers - 1].minBitrate);
    return min_bitrate * scale_factor;
  }

  RTC_DCHECK(codec.mode == VideoCodecMode::kScreensharing);

  DataRate min_bitrate = DataRate::Zero();
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers - 1; ++sl_idx) {
    min_bitrate += DataRate::kbps(codec.spatialLayers[sl_idx].targetBitrate);
  }
  min_bitrate +=
      DataRate::kbps(codec.spatialLayers[num_spatial_layers - 1].minBitrate);

  return min_bitrate;
}

}  // namespace webrtc
