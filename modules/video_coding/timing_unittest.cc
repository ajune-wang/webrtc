/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const int kFps = 25;
}  // namespace

TEST(ReceiverTiming, Tests) {
  SimulatedClock clock(0);
  VCMTiming timing(&clock);
  timing.Reset();

  // Initial target:10 ms (jitter:0 + decode:0 + render:10 (default)).
  EXPECT_EQ(VCMTiming::kDefaultRenderDelayMs, timing.TargetVideoDelay());
  uint32_t timestamp = 0;
  timing.UpdateCurrentDelay(timestamp);  // target:10 -> cur:10

  timing.Reset();  // cur:0, render:10 (default)
  timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());

  // Set jitter delay.
  // target:30 ms (jitter:20 + decode:0 + render:10)
  uint32_t jitter_delay_ms = 20;
  timing.SetJitterDelay(jitter_delay_ms);  // jitter:20 -> cur:20
  timing.UpdateCurrentDelay(timestamp);    // cur:20, tar:30 -> cur:20 (max:0)
  // wait_time:20 ms = (render_time - now):20 - decode:0 - render:0
  timing.set_render_delay(0);
  uint32_t wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),  // cur:20
      clock.TimeInMilliseconds());
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  // Set jitter delay above kDelayMaxChangeMsPerS.
  // target:130 ms (jitter:130 + decode:0 + render:0)
  jitter_delay_ms += VCMTiming::kDelayMaxChangeMsPerS + 10;
  timing.SetJitterDelay(jitter_delay_ms);
  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.UpdateCurrentDelay(timestamp);  // cur:20, tar:130 -> cur:120 (max:100)
  // wait_time:120 ms = (render_time-now):120 - decode:0 - render:0
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),  // cur:120
      clock.TimeInMilliseconds());
  EXPECT_EQ(jitter_delay_ms - 10, wait_time_ms);

  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.UpdateCurrentDelay(timestamp);  // cur:120, tar:130 -> cur:130
  // wait_time:130 ms = (render_time - now):130 - decode:0 - render:0
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),  // cur:130
      clock.TimeInMilliseconds());
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  // Insert frames without jitter, verify that this gives the exact wait time.
  const int kNumFrames = 300;
  for (int i = 0; i < kNumFrames; i++) {
    clock.AdvanceTimeMilliseconds(1000 / kFps);
    timestamp += 90000 / kFps;
    timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
  }
  timing.UpdateCurrentDelay(timestamp);  // target:130 -> cur:130
  // wait_time:130 ms = (render_time - now):130 - decode:0 - render:0
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),  // cur:130
      clock.TimeInMilliseconds());
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  // Add decode time estimates for 1 second.
  // target:140 ms (jitter:130 + decode:10 + render:0)
  const uint32_t kDecodeTimeMs = 10;
  for (int i = 0; i < kFps; i++) {
    clock.AdvanceTimeMilliseconds(kDecodeTimeMs);
    timing.StopDecodeTimer(
        timestamp, kDecodeTimeMs, clock.TimeInMilliseconds(),
        timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()));
    timestamp += 90000 / kFps;
    clock.AdvanceTimeMilliseconds(1000 / kFps - kDecodeTimeMs);
    timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
  }
  timing.UpdateCurrentDelay(timestamp);  // cur:130, tar:140 -> cur:140
  // wait_time:130 ms = (render_time - now):140 - decode:10 - render:0
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),  // cur:140
      clock.TimeInMilliseconds());
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  // Add render delay.
  // target:150 ms (jitter:130 + decode:10 + render:10)
  const int kRenderDelayMs = 10;
  timing.set_render_delay(kRenderDelayMs);

  // Set min playout delay.
  const int kMinTotalDelayMs = 200;
  timing.set_min_playout_delay(kMinTotalDelayMs);
  EXPECT_EQ(kMinTotalDelayMs, timing.TargetVideoDelay());
  clock.AdvanceTimeMilliseconds(1000);
  timestamp += 90000;
  timing.UpdateCurrentDelay(timestamp);  // cur:140, tar:200 -> cur:200
  // wait_time:180 ms = (render_time - now):200 - decode:10 - render:10
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),  // cur:200
      clock.TimeInMilliseconds());
  EXPECT_EQ(kMinTotalDelayMs - kDecodeTimeMs - kRenderDelayMs, wait_time_ms);

  // Reset playout delay.
  timing.set_min_playout_delay(0);
  clock.AdvanceTimeMilliseconds(1000);
  timestamp += 90000;
  timing.UpdateCurrentDelay(timestamp);  // cur:200, tar:150 -> cur:150
  // wait_time:130 ms = (render_time - now):150 - decode:10 - render:10
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),  // cur:150
      clock.TimeInMilliseconds());
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);
}

TEST(ReceiverTiming, WrapAround) {
  SimulatedClock clock(0);
  VCMTiming timing(&clock);
  // Provoke a wrap-around. The fifth frame will have wrapped at 25 fps.
  uint32_t timestamp = 0xFFFFFFFFu - 3 * 90000 / kFps;
  for (int i = 0; i < 5; ++i) {
    timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
    clock.AdvanceTimeMilliseconds(1000 / kFps);
    timestamp += 90000 / kFps;
    EXPECT_EQ(3 * 1000 / kFps,
              timing.RenderTimeMs(0xFFFFFFFFu, clock.TimeInMilliseconds()));
    EXPECT_EQ(3 * 1000 / kFps + 1,
              timing.RenderTimeMs(89u,  // One ms later in 90 kHz.
                                  clock.TimeInMilliseconds()));
  }
}

}  // namespace webrtc
