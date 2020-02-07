/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/overuse_frame_detector.h"

#include <memory>

#include "api/video/i420_buffer.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/event.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/random.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::InvokeWithoutArgs;

namespace {
// Corresponds to load of 15%
const int kFrameIntervalUs = 33 * rtc::kNumMicrosecsPerMillisec;
const int kProcessTimeUs = 5 * rtc::kNumMicrosecsPerMillisec;
}  // namespace

class MockCpuOveruseObserver : public AdaptationObserverInterface {
 public:
  MockCpuOveruseObserver() {}
  virtual ~MockCpuOveruseObserver() {}

  MOCK_METHOD1(AdaptUp, void(AdaptReason));
  MOCK_METHOD1(AdaptDown, bool(AdaptReason));
};

class CpuOveruseObserverImpl : public AdaptationObserverInterface {
 public:
  CpuOveruseObserverImpl() : overuse_(0), normaluse_(0) {}
  virtual ~CpuOveruseObserverImpl() {}

  bool AdaptDown(AdaptReason) override {
    ++overuse_;
    return true;
  }
  void AdaptUp(AdaptReason) override { ++normaluse_; }

  int overuse_;
  int normaluse_;
};

class OveruseFrameDetectorUnderTest : public OveruseFrameDetector {
 public:
  explicit OveruseFrameDetectorUnderTest(
      CpuOveruseMetricsObserver* metrics_observer)
      : OveruseFrameDetector(metrics_observer) {}
  ~OveruseFrameDetectorUnderTest() {}

  using OveruseFrameDetector::CheckForOveruse;
  using OveruseFrameDetector::SetOptions;
};

class OveruseFrameDetectorTest : public ::testing::Test,
                                 public CpuOveruseMetricsObserver {
 protected:
  void SetUp() override {
    observer_ = &mock_observer_;
    options_.min_process_count = 0;
    options_.filter_time_ms = 5 * rtc::kNumMillisecsPerSec;
    overuse_detector_ = std::make_unique<OveruseFrameDetectorUnderTest>(this);
    // Unfortunately, we can't call SetOptions here, since that would break
    // single-threading requirements in the RunOnTqNormalUsage test.
  }

  void OnEncodedFrameTimeMeasured(int encode_time_ms,
                                  int encode_usage_percent) override {
    encode_usage_percent_ = encode_usage_percent;
  }

  int InitialUsage() {
    return ((options_.low_encode_usage_threshold_percent +
             options_.high_encode_usage_threshold_percent) /
            2.0f) +
           0.5;
  }

  virtual void InsertAndSendFramesWithInterval(int num_frames,
                                               int interval_us,
                                               int delay_us) {
    while (num_frames-- > 0) {
      int64_t capture_time_us = rtc::TimeMicros();
      overuse_detector_->FrameSent(capture_time_us, delay_us);
      clock_.AdvanceTime(TimeDelta::us(interval_us));
    }
  }

  virtual void InsertAndSendSimulcastFramesWithInterval(
      int num_frames,
      int interval_us,
      // One element per layer
      rtc::ArrayView<const int> delays_us) {
    while (num_frames-- > 0) {
      int64_t capture_time_us = rtc::TimeMicros();
      int max_delay_us = 0;
      for (int delay_us : delays_us) {
        if (delay_us > max_delay_us) {
          clock_.AdvanceTime(TimeDelta::us(delay_us - max_delay_us));
          max_delay_us = delay_us;
        }

        overuse_detector_->FrameSent(capture_time_us, delay_us);
      }
      overuse_detector_->CheckForOveruse(observer_);
      clock_.AdvanceTime(TimeDelta::us(interval_us - max_delay_us));
    }
  }

  virtual void InsertAndSendFramesWithRandomInterval(int num_frames,
                                                     int min_interval_us,
                                                     int max_interval_us,
                                                     int delay_us) {
    webrtc::Random random(17);

    for (int i = 0; i < num_frames; i++) {
      int interval_us = random.Rand(min_interval_us, max_interval_us);
      int64_t capture_time_us = rtc::TimeMicros();
      overuse_detector_->FrameSent(capture_time_us, delay_us);

      overuse_detector_->CheckForOveruse(observer_);
      clock_.AdvanceTime(TimeDelta::us(interval_us));
    }
  }

  virtual void ForceUpdate() {
    // This is mainly to check initial values and whether the overuse
    // detector has been reset or not.
    InsertAndSendFramesWithInterval(1, rtc::kNumMicrosecsPerSec,
                                    kFrameIntervalUs);
  }
  void TriggerOveruse(int num_times) {
    const int kDelayUs = 32 * rtc::kNumMicrosecsPerMillisec;
    for (int i = 0; i < num_times; ++i) {
      InsertAndSendFramesWithInterval(1000, kFrameIntervalUs, kDelayUs);
      overuse_detector_->CheckForOveruse(observer_);
    }
  }

  void TriggerUnderuse() {
    const int kDelayUs1 = 5000;
    const int kDelayUs2 = 6000;
    InsertAndSendFramesWithInterval(1300, kFrameIntervalUs, kDelayUs1);
    InsertAndSendFramesWithInterval(1, kFrameIntervalUs, kDelayUs2);
    overuse_detector_->CheckForOveruse(observer_);
  }

  int UsagePercent() { return encode_usage_percent_; }

  int64_t OveruseProcessingTimeLimitForFramerate(int fps) const {
    int64_t frame_interval = rtc::kNumMicrosecsPerSec / fps;
    int64_t max_processing_time_us =
        (frame_interval * options_.high_encode_usage_threshold_percent) / 100;
    return max_processing_time_us;
  }

  int64_t UnderuseProcessingTimeLimitForFramerate(int fps) const {
    int64_t frame_interval = rtc::kNumMicrosecsPerSec / fps;
    int64_t max_processing_time_us =
        (frame_interval * options_.low_encode_usage_threshold_percent) / 100;
    return max_processing_time_us;
  }

  CpuOveruseOptions options_;
  rtc::ScopedFakeClock clock_;
  MockCpuOveruseObserver mock_observer_;
  AdaptationObserverInterface* observer_;
  std::unique_ptr<OveruseFrameDetectorUnderTest> overuse_detector_;
  int encode_usage_percent_ = -1;

  static const auto reason_ = AdaptationObserverInterface::AdaptReason::kCpu;
};

// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  // usage > high => overuse
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(reason_)).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  // usage > high => overuse
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(reason_)).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(mock_observer_, AdaptUp(reason_)).Times(::testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(reason_)).Times(2);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(mock_observer_, AdaptUp(reason_)).Times(::testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, TriggerUnderuseWithMinProcessCount) {
  const int kProcessIntervalUs = 5 * rtc::kNumMicrosecsPerSec;
  options_.min_process_count = 1;
  CpuOveruseObserverImpl overuse_observer;
  observer_ = nullptr;
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(1200, kFrameIntervalUs, kProcessTimeUs);
  overuse_detector_->CheckForOveruse(&overuse_observer);
  EXPECT_EQ(0, overuse_observer.normaluse_);
  clock_.AdvanceTime(TimeDelta::us(kProcessIntervalUs));
  overuse_detector_->CheckForOveruse(&overuse_observer);
  EXPECT_EQ(1, overuse_observer.normaluse_);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptUp(reason_)).Times(0);
  EXPECT_CALL(mock_observer_, AdaptDown(reason_)).Times(64);
  for (size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest, ConsecutiveCountTriggersOveruse) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(reason_)).Times(1);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest, IncorrectConsecutiveCountTriggersNoOveruse) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(reason_)).Times(0);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest, ProcessingUsage) {
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(1000, kFrameIntervalUs, kProcessTimeUs);
  EXPECT_EQ(kProcessTimeUs * 100 / kFrameIntervalUs, UsagePercent());
}

#if 0
TEST_F(OveruseFrameDetectorTest, ResetAfterResolutionChange) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate();
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(1000, kFrameIntervalUs,
                                  kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset (with new width/height).
  ForceUpdate(kWidth, kHeight + 1);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}
#endif
TEST_F(OveruseFrameDetectorTest, ResetAfterFrameTimeout) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate();
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(1000, kFrameIntervalUs, kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      2, options_.frame_timeout_interval_ms * rtc::kNumMicrosecsPerMillisec,
      kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset.
  InsertAndSendFramesWithInterval(
      2,
      (options_.frame_timeout_interval_ms + 1) * rtc::kNumMicrosecsPerMillisec,
      kProcessTimeUs);
  ForceUpdate();
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ConvergesSlowly) {
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(1, kFrameIntervalUs, kProcessTimeUs);
  // No update for the first sample.
  EXPECT_EQ(InitialUsage(), UsagePercent());

  // Total time approximately 40 * 33ms = 1.3s, significantly less
  // than the 5s time constant.
  InsertAndSendFramesWithInterval(40, kFrameIntervalUs, kProcessTimeUs);

  // Should have started to approach correct load of 15%, but not very far.
  EXPECT_LT(UsagePercent(), InitialUsage());
  EXPECT_GT(UsagePercent(), (InitialUsage() * 3 + 15) / 4);

  // Run for roughly 10s more, should now be closer.
  InsertAndSendFramesWithInterval(300, kFrameIntervalUs, kProcessTimeUs);
  EXPECT_NEAR(UsagePercent(), 20, 5);
}

TEST_F(OveruseFrameDetectorTest, InitialProcessingUsage) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate();
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, MeasuresMultipleConcurrentSamples) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(reason_)).Times(::testing::AtLeast(1));
  static const int kIntervalUs = 33 * rtc::kNumMicrosecsPerMillisec;
  static const size_t kNumFramesEncodingDelay = 3;
  for (size_t i = 0; i < 1000; ++i) {
    int64_t capture_time_us = rtc::TimeMicros();
    clock_.AdvanceTime(TimeDelta::us(kIntervalUs));
    if (i > kNumFramesEncodingDelay) {
      overuse_detector_->FrameSent(capture_time_us, kIntervalUs);
    }
    overuse_detector_->CheckForOveruse(observer_);
  }
}

TEST_F(OveruseFrameDetectorTest, UpdatesExistingSamples) {
  // >85% encoding time should trigger overuse.
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(reason_)).Times(::testing::AtLeast(1));
  static const int kIntervalUs = 33 * rtc::kNumMicrosecsPerMillisec;
  static const int kDelayUs = 30 * rtc::kNumMicrosecsPerMillisec;
  for (size_t i = 0; i < 1000; ++i) {
    int64_t capture_time_us = rtc::TimeMicros();
    // Encode and send first parts almost instantly.
    clock_.AdvanceTime(TimeDelta::ms(1));
    overuse_detector_->FrameSent(capture_time_us,
                                 rtc::kNumMicrosecsPerMillisec);
    // Encode heavier part, resulting in >85% usage total.
    clock_.AdvanceTime(TimeDelta::us(kDelayUs) - TimeDelta::ms(1));
    overuse_detector_->FrameSent(capture_time_us, kDelayUs);
    clock_.AdvanceTime(TimeDelta::us(kIntervalUs - kDelayUs));
    overuse_detector_->CheckForOveruse(observer_);
  }
}

TEST_F(OveruseFrameDetectorTest, RunOnTqNormalUsage) {
  TaskQueueForTest queue("OveruseFrameDetectorTestQueue");

  queue.SendTask(
      [&] {
        overuse_detector_->StartCheckForOveruse(queue.Get(), options_,
                                                observer_);
      },
      RTC_FROM_HERE);

  rtc::Event event;
  // Expect NormalUsage(). When called, stop the |overuse_detector_| and then
  // set |event| to end the test.
  EXPECT_CALL(mock_observer_, AdaptUp(reason_))
      .WillOnce(InvokeWithoutArgs([this, &event] {
        overuse_detector_->StopCheckForOveruse();
        event.Set();
      }));

  queue.PostTask([this] {
    const int kDelayUs1 = 5 * rtc::kNumMicrosecsPerMillisec;
    const int kDelayUs2 = 6 * rtc::kNumMicrosecsPerMillisec;
    InsertAndSendFramesWithInterval(1300, kFrameIntervalUs, kDelayUs1);
    InsertAndSendFramesWithInterval(1, kFrameIntervalUs, kDelayUs2);
  });

  EXPECT_TRUE(event.Wait(10000));
}

// Models screencast, with irregular arrival of frames which are heavy
// to encode.
TEST_F(OveruseFrameDetectorTest, NoOveruseForLargeRandomFrameInterval) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(_)).Times(0);
  EXPECT_CALL(mock_observer_, AdaptUp(reason_)).Times(::testing::AtLeast(1));

  const int kNumFrames = 500;
  const int kEncodeTimeUs = 100 * rtc::kNumMicrosecsPerMillisec;

  const int kMinIntervalUs = 30 * rtc::kNumMicrosecsPerMillisec;
  const int kMaxIntervalUs = 1000 * rtc::kNumMicrosecsPerMillisec;

  InsertAndSendFramesWithRandomInterval(kNumFrames, kMinIntervalUs,
                                        kMaxIntervalUs, kEncodeTimeUs);
  // Average usage 19%. Check that estimate is in the right ball park.
  EXPECT_NEAR(UsagePercent(), 20, 10);
}

// Models screencast, with irregular arrival of frames, often
// exceeding the timeout interval.
TEST_F(OveruseFrameDetectorTest, NoOveruseForRandomFrameIntervalWithReset) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(_)).Times(0);
  EXPECT_CALL(mock_observer_, AdaptUp(reason_)).Times(::testing::AtLeast(1));

  const int kNumFrames = 500;
  const int kEncodeTimeUs = 100 * rtc::kNumMicrosecsPerMillisec;

  const int kMinIntervalUs = 30 * rtc::kNumMicrosecsPerMillisec;
  const int kMaxIntervalUs = 3000 * rtc::kNumMicrosecsPerMillisec;

  InsertAndSendFramesWithRandomInterval(kNumFrames, kMinIntervalUs,
                                        kMaxIntervalUs, kEncodeTimeUs);

  // Average usage 6.6%, but since the frame_timeout_interval_ms is
  // only 1500 ms, we often reset the estimate to the initial value.
  // Check that estimate is in the right ball park.
  EXPECT_GE(UsagePercent(), 1);
  EXPECT_LE(UsagePercent(), InitialUsage() + 5);
}

TEST_F(OveruseFrameDetectorTest, ToleratesOutOfOrderFrames) {
  overuse_detector_->SetOptions(options_);
  // Represents a cpu utilization close to 100%. First input frame results in
  // three encoded frames, and the last of those isn't finished until after the
  // first encoded frame corresponding to the next input frame.
  const int kEncodeTimeUs = 30 * rtc::kNumMicrosecsPerMillisec;
  const int kCaptureTimesMs[] = {33, 33, 66, 33};

  for (int capture_time_ms : kCaptureTimesMs) {
    overuse_detector_->FrameSent(
        capture_time_ms * rtc::kNumMicrosecsPerMillisec, kEncodeTimeUs);
  }
  EXPECT_GE(UsagePercent(), InitialUsage());
}

// Models simulcast, with multiple encoded frames for each input frame.
// Load estimate should be based on the maximum encode time per input frame.
TEST_F(OveruseFrameDetectorTest, NoOveruseForSimulcast) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown(_)).Times(0);

  constexpr int kNumFrames = 500;
  constexpr int kEncodeTimesUs[] = {
      10 * rtc::kNumMicrosecsPerMillisec,
      8 * rtc::kNumMicrosecsPerMillisec,
      12 * rtc::kNumMicrosecsPerMillisec,
  };
  constexpr int kIntervalUs = 30 * rtc::kNumMicrosecsPerMillisec;

  InsertAndSendSimulcastFramesWithInterval(kNumFrames, kIntervalUs,
                                           kEncodeTimesUs);

  // Average usage 40%. 12 ms / 30 ms.
  EXPECT_GE(UsagePercent(), 35);
  EXPECT_LE(UsagePercent(), 45);
}

}  // namespace webrtc
