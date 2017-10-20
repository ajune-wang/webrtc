/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PRIORITYBASEDALLOCATIONSTRATEGY_H_
#define RTC_BASE_PRIORITYBASEDALLOCATIONSTRATEGY_H_

#include <vector>

#include "rtc_base/bitrateallocationstrategy.h"
#include "rtc_base/checks.h"

namespace rtc {

// Allocation strategy to allocate bitrates to tracks based upon the relative
// bitrate priority for each track.
class PriorityBasedAllocationStrategy
    : public BitrateAllocationStrategy {

 public:
  PriorityBasedAllocationStrategy() {}

  std::vector<uint32_t> AllocateBitrates(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs) override;

 private:
  // Allocate bitrate to tracks when there is not sufficient bitrate to
  // allocate the min bitrate to each track. Allocates to enforced tracks,
  // then to other tracks if there is sufficient remaining.
  std::vector<uint32_t> LowRateAllocationByPriority(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs);
  // Allocates the bitrate based upon the relative_bitrate of each track. This
  // relative bitrate defines the priority for bitrate to be allocated to that
  // track in relation to other tracks. For example with two tracks, if track
  // 1 had a relative_bitrate = 1.0, and track 2 has a relative_bitrate of 2.0,
  // the expected behavior is that track 2 will be allocated double the bitrate
  // as track 1 above the min_bitrate_bps values, until one of the tracks hit
  // its max_bitrate_bps.
  // Pre-condition is that there is available bitrate to allocate the min
  // bitrate of each track.
  std::vector<uint32_t> NormalRateAllocationByPriority(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs);
  // Allocate the max bitrate to each track when there is sufficient available
  // bitrate.
  std::vector<uint32_t> MaxRateAllocation(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs);
  // Allocates each track's bitrate based upon the target bitrate. Each track
  // is allocated the min(max_bitrate_bps, target_bitrate * relative_bitrate).
  std::vector<uint32_t> DistributeBitrateFromTargetBitrate(
      double target_bitrate,
      const ArrayView<const TrackConfig*> track_configs);
};

}  // namespace rtc

#endif  // RTC_BASE_PRIORITYBASEDALLOCATIONSTRATEGY_H_
