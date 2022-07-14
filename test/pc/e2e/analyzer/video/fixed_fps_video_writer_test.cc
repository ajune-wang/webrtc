/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/fixed_fps_video_writer.h"

#include <memory>
#include <vector>

#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/gtest.h"
#include "test/testsupport/video_frame_writer.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr TimeDelta kOneSecond = TimeDelta::Seconds(1);

class InMemoryVideoWriter : public test::VideoFrameWriter {
 public:
  ~InMemoryVideoWriter() override = default;

  bool WriteFrame(const webrtc::VideoFrame& frame) override {
    MutexLock lock(&mutex_);
    received_frames_.push_back(frame);
    return true;
  }

  void Close() override {}

  std::vector<VideoFrame> frames_received() const {
    MutexLock lock(&mutex_);
    return received_frames_;
  }

 private:
  mutable Mutex mutex_;
  std::vector<VideoFrame> received_frames_ RTC_GUARDED_BY(mutex_);
};

VideoFrame EmptyFrameWithId(uint16_t frame_id) {
  return VideoFrame::Builder()
      .set_video_frame_buffer(I420Buffer::Create(1, 1))
      .set_id(frame_id)
      .build();
}

std::unique_ptr<TimeController> CreateSimulatedTimeController() {
  // Using an offset of 100000 to get nice fixed width and readable
  // timestamps in typical test scenarios.
  const Timestamp kSimulatedStartTime = Timestamp::Seconds(100000);
  return std::make_unique<GlobalSimulatedTimeController>(kSimulatedStartTime);
}

TEST(FixedFpsVideoWriterTest, WhenWrittenWithSameFpsVideoIsCorrect) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);

    for (int i = 1; i <= 30; ++i) {
      video_writer.OnFrame(EmptyFrameWithId(i));
      time_controller->AdvanceTime(kOneSecond / fps);
    }
  }

  // First frame is written twice because of a small lag between
  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 30lu);
  for (uint16_t i = 1; i < 30; ++i) {
    EXPECT_EQ(frames_received[i - 1].id(), i);
  }
}

TEST(FixedFpsVideoWriterTest, FrameIsRepeatedWhenThereIsAFreeze) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);

    // Write 10 frames
    for (int i = 1; i <= 10; ++i) {
      video_writer.OnFrame(EmptyFrameWithId(i));
      time_controller->AdvanceTime(kOneSecond / fps);
    }

    // Freeze for 4 frames
    time_controller->AdvanceTime(4 * kOneSecond / fps);

    // Write 10 more frames
    for (int i = 11; i <= 20; ++i) {
      video_writer.OnFrame(EmptyFrameWithId(i));
      time_controller->AdvanceTime(kOneSecond / fps);
    }
  }

  // First frame is written twice because of a small lag between
  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 24lu);

  size_t pos = 0;
  for (uint16_t i = 1; i <= 10; ++i) {
    EXPECT_EQ(frames_received[pos++].id(), i);
  }
  EXPECT_EQ(frames_received[pos++].id(), 10);
  EXPECT_EQ(frames_received[pos++].id(), 10);
  EXPECT_EQ(frames_received[pos++].id(), 10);
  EXPECT_EQ(frames_received[pos++].id(), 10);
  for (uint16_t i = 11; i <= 20; ++i) {
    EXPECT_EQ(frames_received[pos++].id(), i);
  }
}

TEST(FixedFpsVideoWriterTest, NoFramesWritten) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_TRUE(frames_received.empty());
}

TEST(FixedFpsVideoWriterTest,
     FreezeInTheMiddleAndNewFrameReceivedBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 10;  // Inter frame interval is 100ms.

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
    video_writer.OnFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(TimeDelta::Millis(230));
    video_writer.OnFrame(EmptyFrameWithId(2));
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 3lu);
  EXPECT_EQ(frames_received[0].id(), 1);
  EXPECT_EQ(frames_received[1].id(), 1);
  EXPECT_EQ(frames_received[2].id(), 2);
}

TEST(FixedFpsVideoWriterTest,
     FreezeInTheMiddleAndNewFrameReceivedAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 10;  // Inter frame interval is 100ms.

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
    video_writer.OnFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(TimeDelta::Millis(250));
    video_writer.OnFrame(EmptyFrameWithId(2));
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 4lu);
  EXPECT_EQ(frames_received[0].id(), 1);
  EXPECT_EQ(frames_received[1].id(), 1);
  EXPECT_EQ(frames_received[2].id(), 1);
  EXPECT_EQ(frames_received[3].id(), 2);
}

TEST(FixedFpsVideoWriterTest, NewFrameReceivedBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 10;  // Inter frame interval is 100ms.

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
    video_writer.OnFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(TimeDelta::Millis(30));
    video_writer.OnFrame(EmptyFrameWithId(2));
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 1lu);
  EXPECT_EQ(frames_received[0].id(), 2);
}

TEST(FixedFpsVideoWriterTest, NewFrameReceivedAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 10;  // Inter frame interval is 100ms.

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
    video_writer.OnFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(TimeDelta::Millis(50));
    video_writer.OnFrame(EmptyFrameWithId(2));
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 2lu);
  EXPECT_EQ(frames_received[0].id(), 1);
  EXPECT_EQ(frames_received[1].id(), 2);
}

TEST(FixedFpsVideoWriterTest,
     FreeezeAtTheEndAndDestroyBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 10;  // Inter frame interval is 100ms.

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
    video_writer.OnFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(TimeDelta::Millis(230));
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 3lu);
  EXPECT_EQ(frames_received[0].id(), 1);
  EXPECT_EQ(frames_received[1].id(), 1);
  EXPECT_EQ(frames_received[2].id(), 1);
}

TEST(FixedFpsVideoWriterTest,
     FreeezeAtTheEndAndDestroyAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 10;  // Inter frame interval is 100ms.

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
    video_writer.OnFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(TimeDelta::Millis(250));
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 3lu);
  EXPECT_EQ(frames_received[0].id(), 1);
  EXPECT_EQ(frames_received[1].id(), 1);
  EXPECT_EQ(frames_received[2].id(), 1);
}

TEST(FixedFpsVideoWriterTest, DestroyBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 10;  // Inter frame interval is 100ms.

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
    video_writer.OnFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(TimeDelta::Millis(30));
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 1lu);
  EXPECT_EQ(frames_received[0].id(), 1);
}

TEST(FixedFpsVideoWriterTest, DestroyAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 10;  // Inter frame interval is 100ms.

  InMemoryVideoWriter inmemory_writer;

  {
    FixedFpsVideoWriter video_writer(time_controller->GetClock(),
                                     &inmemory_writer, fps);
    video_writer.OnFrame(EmptyFrameWithId(1));
    time_controller->AdvanceTime(TimeDelta::Millis(50));
  }

  std::vector<VideoFrame> frames_received = inmemory_writer.frames_received();
  ASSERT_EQ(frames_received.size(), 1lu);
  EXPECT_EQ(frames_received[0].id(), 1);
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
