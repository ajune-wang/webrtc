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

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const Frequency kFps = Frequency::Hertz(25);
const Frequency kRtp = Frequency::KiloHertz(90);
}  // namespace

TEST(ReceiverTimingTest, JitterDelay) {
  SimulatedClock clock(0);
  VCMTiming timing(&clock);
  timing.Reset();

  uint32_t timestamp = 0;
  timing.UpdateCurrentDelay(timestamp);

  timing.Reset();

  timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  TimeDelta jitter_delay = TimeDelta::Millis(20);
  timing.SetJitterDelay(jitter_delay);
  timing.UpdateCurrentDelay(timestamp);
  timing.set_render_delay(TimeDelta::Zero());
  auto wait_time = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  // First update initializes the render time. Since we have no decode delay
  // we get wait_time = renderTime - now - renderDelay = jitter.
  EXPECT_EQ(jitter_delay, wait_time);

  jitter_delay += TimeDelta::Millis(VCMTiming::kDelayMaxChangeMsPerS + 10);
  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.SetJitterDelay(jitter_delay);
  timing.UpdateCurrentDelay(timestamp);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  // Since we gradually increase the delay we only get 100 ms every second.
  EXPECT_EQ(jitter_delay - TimeDelta::Millis(10), wait_time);

  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.UpdateCurrentDelay(timestamp);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay, wait_time);

  // Insert frames without jitter, verify that this gives the exact wait time.
  const int kNumFrames = 300;
  for (int i = 0; i < kNumFrames; i++) {
    clock.AdvanceTime(1 / kFps);
    timestamp += kRtp / kFps;
    timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  }
  timing.UpdateCurrentDelay(timestamp);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay, wait_time);

  // Add decode time estimates for 1 second.
  const TimeDelta kDecodeTime = TimeDelta::Millis(10);
  for (int i = 0; i < kFps.hertz(); i++) {
    clock.AdvanceTime(kDecodeTime);
    timing.StopDecodeTimer(kDecodeTime, clock.CurrentTime());
    timestamp += kRtp / kFps;
    clock.AdvanceTime(1 / kFps - kDecodeTime);
    timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  }
  timing.UpdateCurrentDelay(timestamp);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay, wait_time);

  const TimeDelta kMinTotalDelayMs = TimeDelta::Millis(200);
  timing.set_min_playout_delay(kMinTotalDelayMs);
  clock.AdvanceTimeMilliseconds(5000);
  timestamp += 5 * 90000;
  timing.UpdateCurrentDelay(timestamp);
  const TimeDelta kRenderDelay = TimeDelta::Millis(10);
  timing.set_render_delay(kRenderDelay);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  // We should at least have kMinTotalDelayMs - decodeTime (10) - renderTime
  // (10) to wait.
  EXPECT_EQ(kMinTotalDelayMs - kDecodeTime - kRenderDelay, wait_time);
  // The total video delay should be equal to the min total delay.
  EXPECT_EQ(kMinTotalDelayMs, timing.TargetVideoDelay());

  // Reset playout delay.
  timing.set_min_playout_delay(TimeDelta::Zero());
  clock.AdvanceTimeMilliseconds(5000);
  timestamp += 5 * 90000;
  timing.UpdateCurrentDelay(timestamp);
}

TEST(ReceiverTimingTest, TimestampWrapAround) {
  constexpr auto kStartTime = Timestamp::Millis(1337);
  SimulatedClock clock(kStartTime);
  VCMTiming timing(&clock);
  // Provoke a wrap-around. The fifth frame will have wrapped at 25 fps.
  uint32_t timestamp = 0xFFFFFFFFu - 3 * kRtp / kFps;
  timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  clock.AdvanceTime(1 / kFps);
  timestamp += kRtp / kFps;
  EXPECT_EQ(kStartTime + 3 / kFps,
            timing.RenderTimeMs(0xFFFFFFFFu, clock.CurrentTime()));
  EXPECT_EQ(kStartTime + 3 / kFps + TimeDelta::Millis(1),
            timing.RenderTimeMs(89u,  // One ms later in 90 kHz.
                                clock.CurrentTime()));
  timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  clock.AdvanceTime(1 / kFps);
  timestamp += kRtp / kFps;
  EXPECT_EQ(kStartTime + 3 / kFps,
            timing.RenderTimeMs(0xFFFFFFFFu, clock.CurrentTime()));
  EXPECT_EQ(kStartTime + 3 / kFps + TimeDelta::Millis(1),
            timing.RenderTimeMs(89u,  // One ms later in 90 kHz.
                                clock.CurrentTime()));
  timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  clock.AdvanceTime(1 / kFps);
  timestamp += kRtp / kFps;
  EXPECT_EQ(kStartTime + 3 / kFps,
            timing.RenderTimeMs(0xFFFFFFFFu, clock.CurrentTime()));
  EXPECT_EQ(kStartTime + 3 / kFps + TimeDelta::Millis(1),
            timing.RenderTimeMs(89u,  // One ms later in 90 kHz.
                                clock.CurrentTime()));
  timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  clock.AdvanceTime(1 / kFps);
  timestamp += kRtp / kFps;
  EXPECT_EQ(kStartTime + 3 / kFps,
            timing.RenderTimeMs(0xFFFFFFFFu, clock.CurrentTime()));
  EXPECT_EQ(kStartTime + 3 / kFps + TimeDelta::Millis(1),
            timing.RenderTimeMs(89u,  // One ms later in 90 kHz.
                                clock.CurrentTime()));
  timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  clock.AdvanceTime(1 / kFps);
  timestamp += kRtp / kFps;
  EXPECT_EQ(kStartTime + 3 / kFps,
            timing.RenderTimeMs(0xFFFFFFFFu, clock.CurrentTime()));
  EXPECT_EQ(kStartTime + 3 / kFps + TimeDelta::Millis(1),
            timing.RenderTimeMs(89u,  // One ms later in 90 kHz.
                                clock.CurrentTime()));
  // for (int i = 0; i < 5; ++i) {
  //   timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  //   clock.AdvanceTime(1 / kFps);
  //   timestamp += kRtp / kFps;
  //   EXPECT_EQ(kStartTime + 3 / kFps,
  //             timing.RenderTimeMs(0xFFFFFFFFu, clock.CurrentTime()));
  //   EXPECT_EQ(kStartTime + 3 / kFps + TimeDelta::Millis(1),
  //             timing.RenderTimeMs(89u,  // One ms later in 90 kHz.
  //                                 clock.CurrentTime()));
  // }
}

TEST(ReceiverTimingTest, MaxWaitingTimeIsZeroForZeroRenderTime) {
  // This is the default path when the RTP playout delay header extension is set
  // to min==0 and max==0.
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
  constexpr Timestamp kZeroRenderTimeMs = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock);
  timing.Reset();
  timing.set_max_playout_delay(TimeDelta::Zero());
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTimeMilliseconds(kTimeDeltaMs);
    Timestamp now = clock.CurrentTime();
    EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now,
                                    /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
  }
  // Another frame submitted at the same time also returns a negative max
  // waiting time.
  Timestamp now = clock.CurrentTime();
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now,
                                  /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  // MaxWaitingTime should be less than zero even if there's a burst of frames.
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now,
                                  /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now,
                                  /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now,
                                  /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
}

TEST(ReceiverTimingTest, MaxWaitingTimeZeroDelayPacingExperiment) {
  // The minimum pacing is enabled by a field trial and active if the RTP
  // playout delay header extension is set to min==0.
  constexpr TimeDelta kMinPacingMs = TimeDelta::Millis(3);
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
  constexpr auto kZeroRenderTime = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock);
  timing.Reset();
  // MaxWaitingTime() returns zero for evenly spaced video frames.
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTimeMilliseconds(kTimeDeltaMs);
    Timestamp now = clock.CurrentTime();
    EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                    /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
    timing.SetLastDecodeScheduledTimestamp(now);
  }
  // Another frame submitted at the same time is paced according to the field
  // trial setting.
  auto now = clock.CurrentTime();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
  // If there's a burst of frames, the wait time is calculated based on next
  // decode time.
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
  // Allow a few ms to pass, this should be subtracted from the MaxWaitingTime.
  constexpr TimeDelta kTwoMs = TimeDelta::Millis(2);
  clock.AdvanceTime(kTwoMs);
  now = clock.CurrentTime();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs - kTwoMs);
  // A frame is decoded at the current time, the wait time should be restored to
  // pacing delay.
  timing.SetLastDecodeScheduledTimestamp(now);
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
}

TEST(ReceiverTimingTest, DefaultMaxWaitingTimeUnaffectedByPacingExperiment) {
  // The minimum pacing is enabled by a field trial but should not have any
  // effect if render_time_ms is greater than 0;
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  const TimeDelta kTimeDelta = TimeDelta::Millis(1000.0 / 60.0);
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock);
  timing.Reset();
  clock.AdvanceTime(kTimeDelta);
  auto now = clock.CurrentTime();
  Timestamp render_time = now + TimeDelta::Millis(30);
  // Estimate the internal processing delay from the first frame.
  TimeDelta estimated_processing_delay =
      (render_time - now) -
      timing.MaxWaitingTime(render_time, now,
                            /*too_many_frames_queued=*/false);
  EXPECT_GT(estimated_processing_delay, TimeDelta::Zero());

  // Any other frame submitted at the same time should be scheduled according to
  // its render time.
  for (int i = 0; i < 5; ++i) {
    render_time += kTimeDelta;
    EXPECT_EQ(timing.MaxWaitingTime(render_time, now,
                                    /*too_many_frames_queued=*/false),
              render_time - now - estimated_processing_delay);
  }
}

TEST(ReceiverTimingTest, MaxWaitingTimeReturnsZeroIfTooManyFramesQueuedIsTrue) {
  // The minimum pacing is enabled by a field trial and active if the RTP
  // playout delay header extension is set to min==0.
  constexpr TimeDelta kMinPacingMs = TimeDelta::Millis(3);
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  const TimeDelta kTimeDelta = TimeDelta::Millis(1000.0 / 60.0);
  constexpr auto kZeroRenderTime = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock);
  timing.Reset();
  // MaxWaitingTime() returns zero for evenly spaced video frames.
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTime(kTimeDelta);
    auto now = clock.CurrentTime();
    EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                    /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
    timing.SetLastDecodeScheduledTimestamp(now);
  }
  // Another frame submitted at the same time is paced according to the field
  // trial setting.
  auto now_ms = clock.CurrentTime();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
  // MaxWaitingTime returns 0 even if there's a burst of frames if
  // too_many_frames_queued is set to true.
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                  /*too_many_frames_queued=*/true),
            TimeDelta::Zero());
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                  /*too_many_frames_queued=*/true),
            TimeDelta::Zero());
}

}  // namespace webrtc
