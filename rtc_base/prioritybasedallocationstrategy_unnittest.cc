/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/gunit.h"
#include "rtc_base/prioritybasedallocationstrategy.h"

namespace {
  constexpr double veryLowRelativeBitrate = 1.0;
  constexpr double lowRelativeBitrate = 2.0;
  constexpr double medRelativeBitrate = 4.0;
  constexpr double highRelativeBitrate = 8.0;
}

namespace rtc {

TEST(PriorityBasedAllocationStrategyTest, MinAllocatedEnforced) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          6000, 10000, true, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          30000, 40000, true, "med", medRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(0, track_config_ptrs);

  EXPECT_EQ(6000u, allocations[0]);
  EXPECT_EQ(30000u, allocations[1]);
}

TEST(PriorityBasedAllocationStrategyTest, MinAllocated) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          6000, 10000, true, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          30000, 40000, false, "med", medRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(36000, track_config_ptrs);

  EXPECT_EQ(6000u, allocations[0]);
  EXPECT_EQ(30000u, allocations[1]);
}

TEST(PriorityBasedAllocationStrategyTest, MinAllocatedByPriority) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, false, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, false, "med", medRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, false, "high", highRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(4000, track_config_ptrs);

  EXPECT_EQ(0u, allocations[0]);
  EXPECT_EQ(2000u, allocations[1]);
  EXPECT_EQ(2000u, allocations[2]);
}

TEST(PriorityBasedAllocationStrategyTest, MinAllocatedEnforcedThenPriority) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, true, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, false, "med", medRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, false, "high", highRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(4000, track_config_ptrs);

  EXPECT_EQ(2000u, allocations[0]);
  EXPECT_EQ(0u, allocations[1]);
  EXPECT_EQ(2000u, allocations[2]);
}

TEST(PriorityBasedAllocationStrategyTest, MinAllocatedThenDistributed) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, true, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, false, "med", medRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          2000, 10000, false, "high", highRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(5000, track_config_ptrs);

  EXPECT_EQ(2500u, allocations[0]);
  EXPECT_EQ(0u, allocations[1]);
  EXPECT_EQ(2500u, allocations[2]);
}

TEST(PriorityBasedAllocationStrategyTest, OneStreamsBasic) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          0, 2000, false, "low", lowRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(1000, track_config_ptrs);

  EXPECT_EQ(1000u, allocations[0]);
}

TEST(PriorityBasedAllocationStrategyTest, TwoStreamsBasic) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          0, 2000, false, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          0, 4000, false, "med", medRelativeBitrate)};
  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(3000, track_config_ptrs);

  EXPECT_EQ(1000u, allocations[0]);
  EXPECT_EQ(2000u, allocations[1]);
}

TEST(PriorityBasedAllocationStrategyTest, TwoStreamsBothAllocatedAboveMin) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          1000, 3000, false, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          2000, 5000, false, "med", medRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(6000, track_config_ptrs);

  EXPECT_EQ(2000u, allocations[0]);
  EXPECT_EQ(4000u, allocations[1]);
}

TEST(PriorityBasedAllocationStrategyTest, TwoStreamsOneAllocatedToMax) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          1000, 4000, false, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          1000, 3000, false, "med", medRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(6000, track_config_ptrs);

  EXPECT_EQ(3000u, allocations[0]);
  EXPECT_EQ(3000u, allocations[1]);
}

TEST(PriorityBasedAllocationStrategyTest, ThreeStreamsOneAllocatedToMax) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          1000, 3000, false, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          1000, 6000, false, "med", medRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          1000, 4000, false, "high", highRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(9000, track_config_ptrs);

  EXPECT_EQ(2000u, allocations[0]);
  EXPECT_EQ(3000u, allocations[1]);
  EXPECT_EQ(4000u, allocations[2]);
}

TEST(PriorityBasedAllocationStrategyTest, ThreeStreamsTwoAllocatedToMax) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          1000, 4000, false, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          1000, 3000, false, "med", medRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          1000, 5000, false, "high", highRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(11000, track_config_ptrs);

  EXPECT_EQ(3000u, allocations[0]);
  EXPECT_EQ(3000u, allocations[1]);
  EXPECT_EQ(5000u, allocations[2]);
}

TEST(PriorityBasedAllocationStrategyTest, FourStreamsBasicAllocation) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          0, 3000, false, "very_low", veryLowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          0, 3000, false, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          0, 6000, false, "med", medRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          0, 10000, false, "high", highRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(15000, track_config_ptrs);

  EXPECT_EQ(1000u, allocations[0]);
  EXPECT_EQ(2000u, allocations[1]);
  EXPECT_EQ(4000u, allocations[2]);
  EXPECT_EQ(8000u, allocations[3]);
}

TEST(PriorityBasedAllocationStrategyTest, MaxAllocated) {
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          6000, 10000, false, "low", lowRelativeBitrate),
      BitrateAllocationStrategy::TrackConfig(
          30000, 40000, false, "med", medRelativeBitrate)};
  std::vector<const BitrateAllocationStrategy::TrackConfig*> track_config_ptrs(
      track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  PriorityBasedAllocationStrategy allocation_strategy;
  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(60000, track_config_ptrs);

  EXPECT_EQ(10000u, allocations[0]);
  EXPECT_EQ(40000u, allocations[1]);
}

}  // namespace rtc
