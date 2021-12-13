/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_scheduler.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "api/video/video_timing.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::SizeIs;

namespace webrtc {

namespace {

constexpr uint32_t kFps30Rtp = 90000 / 30;
constexpr TimeDelta kFps30Delay = 1 / Frequency::Hertz(30);
constexpr uint32_t kFps15Rtp = kFps30Rtp * 2;
const VideoPlayoutDelay kZeroPlayoutDelay = {0, 0};

class FakeEncodedFrame : public EncodedFrame {
 public:
  int64_t ReceivedTime() const override { return 0; }
  int64_t RenderTime() const override { return _renderTimeMs; }
};

MATCHER_P(FrameWithId, id, "") {
  return Matches(Eq(id))(arg->Id());
}

auto FrameWithIds(std::vector<int64_t> ids) {
  std::vector<FrameWithIdMatcherP<int64_t>> matchers;
  for (auto id : ids) {
    matchers.emplace_back(FrameWithId(id));
  }
  return ElementsAreArray(matchers);
}

class Builder {
 public:
  Builder& Time(uint32_t rtp_timestamp) {
    rtp_timestamp_ = rtp_timestamp;
    return *this;
  }
  Builder& Id(int64_t frame_id) {
    frame_id_ = frame_id;
    return *this;
  }
  Builder& AsLast() {
    last_spatial_layer_ = true;
    return *this;
  }
  Builder& Refs(const std::vector<int64_t>& references) {
    references_ = references;
    return *this;
  }
  Builder& PlayoutDelay(VideoPlayoutDelay playout_delay) {
    playout_delay_ = playout_delay;
    return *this;
  }

  std::unique_ptr<FakeEncodedFrame> Build() {
    RTC_CHECK_LE(references_.size(), EncodedFrame::kMaxFrameReferences);
    RTC_CHECK(rtp_timestamp_);
    RTC_CHECK(frame_id_);

    auto frame = std::make_unique<FakeEncodedFrame>();
    frame->SetTimestamp(*rtp_timestamp_);
    frame->SetId(*frame_id_);
    frame->is_last_spatial_layer = last_spatial_layer_;

    if (playout_delay_)
      frame->SetPlayoutDelay(*playout_delay_);

    for (int64_t ref : references_) {
      frame->references[frame->num_references] = ref;
      frame->num_references++;
    }

    return frame;
  }

 private:
  absl::optional<uint32_t> rtp_timestamp_;
  absl::optional<int64_t> frame_id_;
  absl::optional<VideoPlayoutDelay> playout_delay_;
  bool last_spatial_layer_ = false;
  std::vector<int64_t> references_;
};

constexpr auto kMaxWaitForKeyframe = TimeDelta::Millis(500);
constexpr auto kMaxWaitForFrame = TimeDelta::Millis(1500);
const FrameScheduler::Timeouts config = {kMaxWaitForKeyframe, kMaxWaitForFrame};

class FrameSchedulerTest : public ::testing::Test {
 public:
  FrameSchedulerTest()
      : time_controller_(Timestamp::Zero()),
        clock_(time_controller_.GetClock()),
        task_queue_(time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
            "scheduler",
            TaskQueueFactory::Priority::NORMAL)),
        timing_(time_controller_.GetClock()),
        frame_buffer_(200, 200),
        scheduler_(std::make_unique<FrameScheduler>(
            time_controller_.GetClock(),
            &task_queue_,
            &timing_,
            &frame_buffer_,
            config,
            absl::bind_front(&FrameSchedulerTest::OnFrameReady, this),
            absl::bind_front(&FrameSchedulerTest::OnTimeout, this))) {}

  ~FrameSchedulerTest() override = default;

  int timeouts() const { return timeouts_; }

  std::vector<absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4>>& frames() {
    return frames_;
  }

  void TearDown() override {
    if (scheduler_) {
      task_queue_.PostTask([&] { scheduler_->Stop(); });
      time_controller_.AdvanceTime(TimeDelta::Zero());
    }
  }

  template <class Task>
  void OnQueue(Task&& t) {
    task_queue_.PostTask(std::forward<Task>(t));
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

 protected:
  GlobalSimulatedTimeController time_controller_;
  Clock* const clock_;
  rtc::TaskQueue task_queue_;
  VCMTiming timing_;
  FrameBuffer frame_buffer_;
  std::unique_ptr<FrameScheduler> scheduler_;

 private:
  void OnTimeout() { timeouts_++; }

  void OnFrameReady(
      absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames) {
    frames_.push_back(std::move(frames));
  }

  int timeouts_ = 0;
  std::vector<absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4>> frames_;
};

TEST_F(FrameSchedulerTest, InitialTimeoutAfterKeyframeTimeoutPeriod) {
  task_queue_.PostTask([&] { scheduler_->Start(); });
  time_controller_.AdvanceTime(TimeDelta::Zero());

  time_controller_.AdvanceTime(kMaxWaitForKeyframe);
  EXPECT_EQ(timeouts(), 1);
}

TEST_F(FrameSchedulerTest, KeyFramesAreScheduled) {
  task_queue_.PostTask([&] { scheduler_->Start(); });
  time_controller_.AdvanceTime(TimeDelta::Zero());

  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  frame_buffer_.InsertFrame(std::move(frame));
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  // EXPECT_THAT(frames(), IsEmpty());
  // Have to wait for keyframe.
  time_controller_.AdvanceTime(TimeDelta::Zero());
  ASSERT_THAT(frames(), ElementsAre(ElementsAre(FrameWithId(0))));
  EXPECT_EQ(timeouts(), 0);
}

TEST_F(FrameSchedulerTest, DeltaFrameTimeoutAfterKeyframeExtracted) {
  OnQueue([&] { scheduler_->Start(); });

  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  frame_buffer_.InsertFrame(std::move(frame));
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  EXPECT_THAT(frames(), Not(IsEmpty()));

  // Timeouts should now happen at the normal frequency.
  const int expected_timeouts = 5;
  time_controller_.AdvanceTime(kMaxWaitForFrame * expected_timeouts);

  EXPECT_EQ(timeouts(), expected_timeouts);
}

TEST_F(FrameSchedulerTest, DependantFramesAreScheduled) {
  task_queue_.PostTask([&] { scheduler_->Start(); });
  time_controller_.AdvanceTime(TimeDelta::Zero());

  frame_buffer_.InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  EXPECT_THAT(frames(), ElementsAre(ElementsAre(FrameWithId(0))));
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });
  // Wait half fps - insert new frame.
  auto wait = kFps30Delay;
  time_controller_.AdvanceTime(wait);
  frame_buffer_.InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  time_controller_.AdvanceTime(kFps30Delay - wait);

  // 2 Frames - no super frames.
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0}), FrameWithIds({1})));
  EXPECT_EQ(timeouts(), 0);
}

TEST_F(FrameSchedulerTest, SpacialLayersAreScheduled) {
  OnQueue([&] { scheduler_->Start(); });

  frame_buffer_.InsertFrame(Builder().Id(0).Time(0).Build());
  frame_buffer_.InsertFrame(Builder().Id(1).Time(0).Build());
  frame_buffer_.InsertFrame(Builder().Id(2).Time(0).AsLast().Build());
  frame_buffer_.InsertFrame(Builder().Id(3).Time(kFps30Rtp).Refs({0}).Build());
  frame_buffer_.InsertFrame(
      Builder().Id(4).Time(kFps30Rtp).Refs({0, 1}).Build());
  frame_buffer_.InsertFrame(
      Builder().Id(5).Time(kFps30Rtp).Refs({0, 1, 2}).AsLast().Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });
  time_controller_.AdvanceTime(kFps30Delay * 10);

  EXPECT_EQ(timeouts(), 0);
  EXPECT_THAT(frames(),
              ElementsAre(FrameWithIds({0, 1, 2}), FrameWithIds({3, 4, 5})));
}

TEST_F(FrameSchedulerTest, OutstandingFrameTasksAreCancelledAfterDeletion) {
  OnQueue([&] { scheduler_->Start(); });

  frame_buffer_.InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  frame_buffer_.InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  // Get keyframe. Delta frame should now be scheduled.
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_THAT(frames(), SizeIs(1));
  OnQueue([&] {
    scheduler_->OnReadyForNextFrame();
    scheduler_->Stop();
    scheduler_ = nullptr;
  });
  time_controller_.AdvanceTime(kMaxWaitForFrame * 2);
  // Wait for 2x max wait time. Since we stopped, this should cause no timeouts
  // or frame-ready callbacks.
  EXPECT_THAT(frames(), SizeIs(1));
  EXPECT_EQ(timeouts(), 0);
}

TEST_F(FrameSchedulerTest, FramesWaitForDecoderToComplete) {
  OnQueue([&] { scheduler_->Start(); });

  // Start with a keyframe.
  frame_buffer_.InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  EXPECT_THAT(frames(), ElementsAre(ElementsAre(FrameWithId(0))));

  // Insert a delta frame.
  frame_buffer_.InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  // Advancing time should not result in a frame since the scheduler has not
  // been signalled that we are ready.
  time_controller_.AdvanceTime(kFps30Delay);
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0})));
  // Signal ready.
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });

  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0}), FrameWithIds({1})));
}

TEST_F(FrameSchedulerTest, LateFrameDropped) {
  OnQueue([&] { scheduler_->Start(); });
  //   F1
  //   /
  // F0 --> F2
  frame_buffer_.InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  // Start with a keyframe.
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  EXPECT_THAT(frames(), ElementsAre(ElementsAre(FrameWithId(0))));
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });

  // Simulate late F1
  time_controller_.AdvanceTime(kFps30Delay * 2);
  frame_buffer_.InsertFrame(
      Builder().Id(2).Time(2 * kFps30Rtp).AsLast().Refs({0}).Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });

  frame_buffer_.InsertFrame(
      Builder().Id(1).Time(1 * kFps30Rtp).AsLast().Refs({0}).Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  time_controller_.AdvanceTime(kFps30Delay);
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0}), FrameWithIds({2})));

  // Confirm frame 1 is never scheduled by timing out.
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });
  time_controller_.AdvanceTime(kMaxWaitForFrame);
  EXPECT_EQ(timeouts(), 1);
}

TEST_F(FrameSchedulerTest, FramesFastForwardOnSystemHalt) {
  OnQueue([&] { scheduler_->Start(); });
  //   F1
  //   /
  // F0 --> F2
  frame_buffer_.InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  frame_buffer_.InsertFrame(
      Builder().Id(1).Time(1 * kFps30Rtp).AsLast().Refs({0}).Build());
  frame_buffer_.InsertFrame(
      Builder().Id(2).Time(2 * kFps30Rtp).AsLast().Refs({0}).Build());
  // Start with a keyframe.
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0})));
  // Halting time should result in F1 being skipped.
  time_controller_.AdvanceTime(kFps30Delay * 2);
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });

  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0}), FrameWithIds({2})));
}

TEST_F(FrameSchedulerTest, ForceKeyFrame) {
  OnQueue([&] { scheduler_->Start(); });
  // Initial keyframe.
  frame_buffer_.InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0})));
  OnQueue([&] { scheduler_->ForceKeyFrame(); });
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });

  // F2 is the next keyframe, and should be extracted since a keyframe was
  // forced.
  frame_buffer_.InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  frame_buffer_.InsertFrame(
      Builder().Id(2).Time(kFps30Rtp * 2).AsLast().Build());
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });

  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0}), FrameWithIds({2})));
}

TEST_F(FrameSchedulerTest, FirstFrameNonKeyframe) {
  // F1 was not a keyframe, F2 is and should be the frame extracted.
  frame_buffer_.InsertFrame(Builder().Id(1).Time(0).AsLast().Refs({0}).Build());
  frame_buffer_.InsertFrame(
      Builder().Id(2).Time(1 * kFps30Rtp).AsLast().Build());
  OnQueue([&] { scheduler_->Start(); });
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({2})));
}

TEST_F(FrameSchedulerTest, SlowDecoderDropsTemporalLayers) {
  OnQueue([&] { scheduler_->Start(); });
  // 2 temporal layers, at 15fps per layer to make 30fps total.
  // Decoder is slower than 30fps, so frames will be skipped.
  //   F1 --> F3 --> F5
  //   /      /     /
  // F0 --> F2 --> F4
  frame_buffer_.InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  frame_buffer_.InsertFrame(
      Builder().Id(1).Time(1 * kFps15Rtp).Refs({0}).AsLast().Build());
  frame_buffer_.InsertFrame(
      Builder().Id(2).Time(2 * kFps15Rtp).Refs({0}).AsLast().Build());
  frame_buffer_.InsertFrame(
      Builder().Id(3).Time(3 * kFps15Rtp).Refs({1, 2}).AsLast().Build());
  frame_buffer_.InsertFrame(
      Builder().Id(4).Time(4 * kFps15Rtp).Refs({2}).AsLast().Build());
  frame_buffer_.InsertFrame(
      Builder().Id(5).Time(5 * kFps15Rtp).Refs({3, 4}).AsLast().Build());

  const TimeDelta kSlowDecodeDelay = kFps30Delay + TimeDelta::Millis(10);

  // Keyframe received.
  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0})));

  // F2 is the best frame since decoding was so slow that F1 is too old.
  time_controller_.AdvanceTime(kSlowDecodeDelay);
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0}), FrameWithIds({2})));

  // F4 is the best frame since decoding was so slow that F1 is too old.
  time_controller_.AdvanceTime(kSlowDecodeDelay);
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0}), FrameWithIds({2}),
                                    FrameWithIds({4})));

  // F5 is not decodable since F4 was decoded, so a timeout is expected.
  time_controller_.AdvanceTime(kSlowDecodeDelay);
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });
  time_controller_.AdvanceTime(kMaxWaitForFrame);
  EXPECT_EQ(timeouts(), 1);
}

TEST_F(FrameSchedulerTest, OldTimestampNotDecodable) {
  OnQueue([&] { scheduler_->Start(); });

  frame_buffer_.InsertFrame(Builder().Id(0).Time(kFps30Rtp).AsLast().Build());
  // Timestamp is before previous frame's.
  frame_buffer_.InsertFrame(Builder().Id(1).Time(0).AsLast().Build());

  OnQueue([&] { scheduler_->OnFrameBufferChanged(); });
  OnQueue([&] { scheduler_->OnReadyForNextFrame(); });
  // F1 should be dropped since its timestamp went backwards.
  time_controller_.AdvanceTime(kMaxWaitForFrame);
  EXPECT_THAT(frames(), ElementsAre(FrameWithIds({0})));
  EXPECT_EQ(timeouts(), 1);
}

}  // namespace
}  // namespace webrtc
