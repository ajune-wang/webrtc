/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/prioritybasedallocationstrategy.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "rtc_base/checks.h"

namespace rtc {

std::vector<uint32_t> PriorityBasedAllocationStrategy::AllocateBitrates(
    uint32_t available_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*>
        track_configs) {
  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;
  for (const auto* track_config : track_configs) {
    sum_min_bitrates += track_config->min_bitrate_bps;
    sum_max_bitrates += track_config->max_bitrate_bps;
  }

  if (available_bitrate <= sum_min_bitrates)
    return LowRateAllocationByPriority(available_bitrate, track_configs);

  if (available_bitrate < sum_max_bitrates)
    return NormalRateAllocationByPriority(available_bitrate, track_configs);

  return MaxRateAllocation(available_bitrate, track_configs);
}

std::vector<uint32_t>
PriorityBasedAllocationStrategy::LowRateAllocationByPriority(
    uint32_t available_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*>
        track_configs) {
  int64_t remaining_bitrate = available_bitrate;
  // This is needed because tracks get sorted on priority.
  std::map<const BitrateAllocationStrategy::TrackConfig*, uint32_t>
      track_allocation_map;
  std::vector<std::pair<double, const BitrateAllocationStrategy::TrackConfig*>>
      track_priority_config_pairs;
  // First allocate to the tracks that enforce it.
  for (const auto* track_config : track_configs) {
    if (track_config->enforce_min_bitrate) {
      track_allocation_map[track_config] = track_config->min_bitrate_bps;
      remaining_bitrate -= track_config->min_bitrate_bps;
    }
    track_priority_config_pairs.push_back(
        std::pair<double, const BitrateAllocationStrategy::TrackConfig*>(
        track_config->relative_bitrate, track_config));
  }
  // Allocate all other tracks, prioritizing by the tracks with the largest
  // relative bitrate priority.
  std::sort(track_priority_config_pairs.begin(),
            track_priority_config_pairs.end());
  std::reverse(track_priority_config_pairs.begin(),
               track_priority_config_pairs.end());
  for (const auto& track_priority_config_pair : track_priority_config_pairs) {
    const BitrateAllocationStrategy::TrackConfig* track_config =
        track_priority_config_pair.second;
    if (!track_config->enforce_min_bitrate &&
        remaining_bitrate >= track_config->min_bitrate_bps) {
      track_allocation_map[track_config] = track_config->min_bitrate_bps;
      remaining_bitrate -= track_config->min_bitrate_bps;
    }
  }
  // Distribute the remaining bitrate evenly to tracks that have been allocated
  // the min bitrate.
  if (remaining_bitrate > 0) {
    int num_tracks_allocated_min = track_allocation_map.size();
    for (auto& track_allocation : track_allocation_map) {
      uint32_t extra_allocation = remaining_bitrate / num_tracks_allocated_min;
      uint32_t total_allocation = track_allocation.second + extra_allocation;
      if (total_allocation > track_allocation.first->max_bitrate_bps) {
        extra_allocation = track_allocation.first->max_bitrate_bps -
                           track_allocation.second;
        total_allocation = track_allocation.first->max_bitrate_bps;
      }
      track_allocation.second = total_allocation;
      remaining_bitrate -= extra_allocation;
      num_tracks_allocated_min--;
    }
  }
  std::vector<uint32_t> track_allocations;
  for (const auto* track_config : track_configs) {
    track_allocations.push_back(track_allocation_map[track_config]);
  }
  return track_allocations;
}

std::vector<uint32_t>
PriorityBasedAllocationStrategy::NormalRateAllocationByPriority(
    uint32_t available_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*>
        track_configs) {
  uint32_t remaining_bitrate = available_bitrate;
  // Pairs of (scaled_track_bandwidth, relative_bitrate) for each track,
  // where scaled_track_bandwidth =
  // (max bitrate - min bitrate) / relative_bitrate.
  std::vector<std::pair<double, double>> scaled_track_bandwidths;
  // This is the factor multiplied by a given target bitrate range to find
  // how much total bitrate is allocated for that range.
  double track_allocation_factor = 0;
  // The target bitrate is the scaled bitrate allocated to each track above
  // its min bitrate. The default is 0, allocating only the min bitrate.
  double target_bitrate = 0;

  // Calculate scaled_track_bandwidths & update the remaining bitrate.
  for (const auto* track_config : track_configs) {
    remaining_bitrate -= track_config->min_bitrate_bps;
    // Calculate and store the scaled track bandwidth. This is the track's
    // bandwidth available to be allocated then scaled by it's relative_bitrate.
    double relative_bitrate = track_config->relative_bitrate;
    uint32_t bandwidth_range =
        track_config->max_bitrate_bps - track_config->min_bitrate_bps;
    uint32_t scaled_bandwidth =
        static_cast<double>(bandwidth_range) / relative_bitrate;
    scaled_track_bandwidths.push_back(
        std::pair<double, double>(scaled_bandwidth, relative_bitrate));
    // At the start all tracks will get allocated bitrate from remaining
    // bitrate and therefore will contribute to the allocation factor.
    track_allocation_factor += relative_bitrate;
  }

  // Find the target bitrate point where bitrate can no longer be allocated.
  std::sort(scaled_track_bandwidths.begin(), scaled_track_bandwidths.end());
  for (const auto& scaled_track_bandwidth_pair : scaled_track_bandwidths) {
    double next_target_bitrate = scaled_track_bandwidth_pair.first;
    double allocation_range = next_target_bitrate - target_bitrate;
    double allocated_bitrate = track_allocation_factor * allocation_range;
    // We have reached a point where we can calculate the target bitrate.
    if (allocated_bitrate > remaining_bitrate)
      break;
    target_bitrate = next_target_bitrate;
    remaining_bitrate -= allocated_bitrate;
    // This track will no longer be allocated from the remaining bitrate,
    // and therefore it's relative bitrate is taken from the allocation factor.
    track_allocation_factor -= scaled_track_bandwidth_pair.second;
  }
  target_bitrate += remaining_bitrate / track_allocation_factor;
  return DistributeBitrateFromTargetBitrate(target_bitrate, track_configs);
}

std::vector<uint32_t> PriorityBasedAllocationStrategy::MaxRateAllocation(
    uint32_t available_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*>
        track_configs) {
  std::vector<uint32_t> track_allocations;
  for (const auto* track_config : track_configs) {
    track_allocations.push_back(track_config->max_bitrate_bps);
  }
  return track_allocations;
}

std::vector<uint32_t>
PriorityBasedAllocationStrategy::DistributeBitrateFromTargetBitrate(
    double target_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*>
        track_configs) {
  std::vector<uint32_t> track_allocations;
  for (const auto* track_config : track_configs) {
    double target_allocation =
        track_config->relative_bitrate * target_bitrate;
    uint32_t track_allocation = std::min(
        track_config->max_bitrate_bps,
        static_cast<uint32_t>(target_allocation) +
            track_config->min_bitrate_bps);
    track_allocations.push_back(track_allocation);
  }
  return track_allocations;
}

}  // namespace rtc
