/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/metronome_frame_scheduler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "api/metronome/test/fake_metronome.h"
#include "api/units/time_delta.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/time_controller/simulated_time_controller.h"
#include "video/frame_decode_scheduler.h"
#include "video/frame_decode_timing.h"

using ::testing::_;
using ::testing::Eq;

namespace webrtc {

class MockFrameReadyCallback : public FrameDecodeScheduler::ReadyCallback {
 public:
  MOCK_METHOD(void,
              FrameReadyForDecode,
              (uint32_t rtp, Timestamp ts),
              (override));
};

class MetronomeFrameDecodeSchedulerTest : public ::testing::Test {
 public:
  static constexpr TimeDelta kTickPeriod = TimeDelta::Millis(33);

  MetronomeFrameDecodeSchedulerTest()
      : time_controller_(Timestamp::Millis(1337)),
        clock_(time_controller_.GetClock()),
        metronome_(kTickPeriod),
        decode_syncronizer_(clock_, &metronome_, run_loop_.task_queue()) {}

 protected:
  GlobalSimulatedTimeController time_controller_;
  Clock* clock_;
  test::RunLoop run_loop_;
  FakeMetronome metronome_;
  DecodeSyncronrizer decode_syncronizer_;
};

TEST_F(MetronomeFrameDecodeSchedulerTest, AllFramesReadyBeforeNextTickDecoded) {
  auto mock_callback1 = ::testing::StrictMock<MockFrameReadyCallback>();
  auto scheduler1 = decode_syncronizer_.AddReceiveStream(&mock_callback1);

  auto mock_callback2 = ::testing::StrictMock<MockFrameReadyCallback>();
  auto scheduler2 = decode_syncronizer_.AddReceiveStream(&mock_callback2);

  {
    uint32_t frame_rtp = 90000;
    FrameDecodeTiming::FrameSchedule frame_sched{
        .max_decode_time = clock_->CurrentTime() + TimeDelta::Millis(10),
        .render_time = clock_->CurrentTime() + TimeDelta::Millis(30)};
    scheduler1->ScheduleFrame(frame_rtp, frame_sched);
    EXPECT_CALL(
        mock_callback1,
        FrameReadyForDecode(Eq(frame_rtp), Eq(frame_sched.render_time)));
  }
  {
    uint32_t frame_rtp = 123456;
    FrameDecodeTiming::FrameSchedule frame_sched{
        .max_decode_time = clock_->CurrentTime() + TimeDelta::Millis(13),
        .render_time = clock_->CurrentTime() + TimeDelta::Millis(33)};
    scheduler2->ScheduleFrame(frame_rtp, frame_sched);
    EXPECT_CALL(
        mock_callback2,
        FrameReadyForDecode(Eq(frame_rtp), Eq(frame_sched.render_time)));
  }
  metronome_.Tick();
  run_loop_.Flush();

  // Cleanup
  decode_syncronizer_.RemoveStream(&mock_callback1, std::move(scheduler1));
  decode_syncronizer_.RemoveStream(&mock_callback2, std::move(scheduler2));
}

TEST_F(MetronomeFrameDecodeSchedulerTest,
       FramesNotDecodedIfDecodeTimeIsInNextInterval) {
  auto mock_callback = ::testing::StrictMock<MockFrameReadyCallback>();
  auto scheduler = decode_syncronizer_.AddReceiveStream(&mock_callback);

  uint32_t frame_rtp = 90000;
  FrameDecodeTiming::FrameSchedule frame_sched{
      .max_decode_time =
          clock_->CurrentTime() + kTickPeriod + TimeDelta::Millis(10),
      .render_time =
          clock_->CurrentTime() + kTickPeriod + TimeDelta::Millis(30)};
  scheduler->ScheduleFrame(frame_rtp, frame_sched);

  metronome_.Tick();
  run_loop_.Flush();
  // No decodes should have happened in this tick.
  ::testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Decode should happen on next tick.
  EXPECT_CALL(mock_callback,
              FrameReadyForDecode(Eq(frame_rtp), Eq(frame_sched.render_time)));
  time_controller_.AdvanceTime(kTickPeriod);
  metronome_.Tick();
  run_loop_.Flush();

  // Cleanup
  decode_syncronizer_.RemoveStream(&mock_callback, std::move(scheduler));
}

TEST_F(MetronomeFrameDecodeSchedulerTest, FrameDecodedOnce) {
  auto mock_callback = ::testing::StrictMock<MockFrameReadyCallback>();
  auto scheduler = decode_syncronizer_.AddReceiveStream(&mock_callback);

  uint32_t frame_rtp = 90000;
  FrameDecodeTiming::FrameSchedule frame_sched{
      .max_decode_time = clock_->CurrentTime() + TimeDelta::Millis(10),
      .render_time = clock_->CurrentTime() + TimeDelta::Millis(30)};
  scheduler->ScheduleFrame(frame_rtp, frame_sched);
  EXPECT_CALL(mock_callback, FrameReadyForDecode(_, _)).Times(1);
  metronome_.Tick();
  run_loop_.Flush();
  ::testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Trigger tick again. No frame should be decoded now.
  time_controller_.AdvanceTime(kTickPeriod);
  metronome_.Tick();
  run_loop_.Flush();

  // Cleanup
  decode_syncronizer_.RemoveStream(&mock_callback, std::move(scheduler));
}

}  // namespace webrtc
