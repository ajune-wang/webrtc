/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/fixed_fps_video_frame_writer_adaptor.h"

#include <memory>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/video_frame_writer.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace test {
namespace {

constexpr TimeDelta kOneSecond = TimeDelta::Seconds(1);

using ::testing::ElementsAre;

struct TimedFrame {
  VideoFrame frame;
  Timestamp time;
};

class InMemoryVideoWriter : public test::VideoFrameWriter {
 public:
  explicit InMemoryVideoWriter(Clock* clock) : clock_(clock) {}
  ~InMemoryVideoWriter() override = default;

  bool WriteFrame(const webrtc::VideoFrame& frame) override {
    MutexLock lock(&mutex_);
    received_frames_.push_back({.frame = frame, .time = clock_->CurrentTime()});
    return true;
  }

  void Close() override {}

  std::vector<TimedFrame> received_frames() const {
    MutexLock lock(&mutex_);
    return received_frames_;
  }

 private:
  Clock* const clock_;
  mutable Mutex mutex_;
  std::vector<TimedFrame> received_frames_ RTC_GUARDED_BY(mutex_);
};

VideoFrame EmptyFrameWithId(uint16_t frame_id) {
  return VideoFrame::Builder()
      .set_video_frame_buffer(I420Buffer::Create(1, 1))
      .set_id(frame_id)
      .build();
}

std::vector<uint16_t> FrameIds(const std::vector<TimedFrame>& frames) {
  std::vector<uint16_t> out;
  for (const TimedFrame& frame : frames) {
    out.push_back(frame.frame.id());
  }
  return out;
}

std::vector<TimeDelta> InterframeIntervals(
    const std::vector<TimedFrame>& frames) {
  std::vector<TimeDelta> out;
  for (size_t i = 1; i < frames.size(); ++i) {
    out.push_back(frames[i].time - frames[i - 1].time);
  }
  return out;
}

std::unique_ptr<TimeController> CreateSimulatedTimeController() {
  // Using an offset of 100000 to get nice fixed width and readable
  // timestamps in typical test scenarios.
  const Timestamp kSimulatedStartTime = Timestamp::Seconds(100000);
  return std::make_unique<GlobalSimulatedTimeController>(kSimulatedStartTime);
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     WhenWrittenWithSameFpsVideoIsCorrect) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        fps, time_controller->GetClock(), &inmemory_writer);

    for (int i = 1; i <= 30; ++i) {
      video_writer.WriteFrame(EmptyFrameWithId(i));
      time_controller->AdvanceTime(kOneSecond / fps);
    }
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(
      FrameIds(received_frames),
      ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                  19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest, FrameIsRepeatedWhenThereIsAFreeze) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        fps, time_controller->GetClock(), &inmemory_writer);

    // Write 10 frames
    for (int i = 1; i <= 10; ++i) {
      video_writer.WriteFrame(EmptyFrameWithId(i));
      time_controller->AdvanceTime(kOneSecond / fps);
    }

    // Freeze for 4 frames
    time_controller->AdvanceTime(4 * kOneSecond / fps);

    // Write 10 more frames
    for (int i = 11; i <= 20; ++i) {
      video_writer.WriteFrame(EmptyFrameWithId(i));
      time_controller->AdvanceTime(kOneSecond / fps);
    }
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames),
              ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10, 10, 11, 12,
                          13, 14, 15, 16, 17, 18, 19, 20));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest, NoFramesWritten) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        fps, time_controller->GetClock(), &inmemory_writer);
    time_controller->AdvanceTime(TimeDelta::Millis(100));
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  ASSERT_TRUE(received_frames.empty());
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     FreezeInTheMiddleAndNewFrameReceivedBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    video_writer.WriteFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(2.3 * kInterval);
    video_writer.WriteFrame(EmptyFrameWithId(2));
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 1, 2));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     FreezeInTheMiddleAndNewFrameReceivedAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    video_writer.WriteFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(2.5 * kInterval);
    video_writer.WriteFrame(EmptyFrameWithId(2));
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 1, 1, 2));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     NewFrameReceivedBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    video_writer.WriteFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(0.3 * kInterval);
    video_writer.WriteFrame(EmptyFrameWithId(2));
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(2));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     NewFrameReceivedAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    video_writer.WriteFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(0.5 * kInterval);
    video_writer.WriteFrame(EmptyFrameWithId(2));
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 2));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     FreeezeAtTheEndAndDestroyBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    video_writer.WriteFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(2.3 * kInterval);
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 1, 1));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     FreeezeAtTheEndAndDestroyAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    video_writer.WriteFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(2.5 * kInterval);
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 1, 1));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     DestroyBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    video_writer.WriteFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(0.3 * kInterval);
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest,
     DestroyAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    video_writer.WriteFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(0.5 * kInterval);
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1));
}

TEST(FixedFpsVideoFrameWriterAdaptorTest, InterFrameIntervalsAreEqual) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  InMemoryVideoWriter inmemory_writer(time_controller->GetClock());

  {
    FixedFpsVideoFrameWriterAdaptor video_writer(
        kFps, time_controller->GetClock(), &inmemory_writer);
    uint16_t frame_id = 1;
    video_writer.WriteFrame(EmptyFrameWithId(frame_id++));
    for (int i = 0; i < 5; ++i) {
      time_controller->AdvanceTime(0.3 * kInterval);
      video_writer.WriteFrame(EmptyFrameWithId(frame_id++));
      time_controller->AdvanceTime(0.5 * kInterval);
      video_writer.WriteFrame(EmptyFrameWithId(frame_id++));
      time_controller->AdvanceTime(0.2 * kInterval);
    }
  }

  std::vector<TimedFrame> received_frames = inmemory_writer.received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(2, 4, 6, 8, 10, 11));
  // Last interval is shorter, because `video_writer` was destroyed after
  // 0.2 * kInterval and it lead to the flush of frames.
  EXPECT_THAT(
      InterframeIntervals(received_frames),
      ElementsAre(kInterval, kInterval, kInterval, kInterval, 0.2 * kInterval));
}

}  // namespace
}  // namespace test
}  // namespace webrtc
