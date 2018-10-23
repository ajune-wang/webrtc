/*  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/jitter_estimator.h"

#include "rtc_base/experiments/jitter_upper_bound_experiment.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

class TestVCMJitterEstimator : public ::testing::Test {
 protected:
  TestVCMJitterEstimator() : fake_clock_(0) {}

  virtual void SetUp() {
    estimator_ = absl::make_unique<VCMJitterEstimator>(&fake_clock_, 0, 0);
  }

  void AdvanceClock(int64_t microseconds) {
    fake_clock_.AdvanceTimeMicroseconds(microseconds);
  }

  SimulatedClock fake_clock_;
  std::unique_ptr<VCMJitterEstimator> estimator_;
};

// Generates some simple test data in the form of a sawtooth wave.
class ValueGenerator {
 public:
  explicit ValueGenerator(int32_t amplitude)
      : amplitude_(amplitude), counter_(0) {}
  virtual ~ValueGenerator() {}

  int64_t Delay() { return ((counter_ % 11) - 5) * amplitude_; }

  uint32_t FrameSize() { return 1000 + Delay(); }

  void Advance() { ++counter_; }

 private:
  const int32_t amplitude_;
  int64_t counter_;
};

// 5 fps, disable jitter delay altogether.
TEST_F(TestVCMJitterEstimator, TestLowRate) {
  ValueGenerator gen(10);
  uint64_t time_delta_us = rtc::kNumMicrosecsPerSec / 5;
  for (int i = 0; i < 60; ++i) {
    estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
    AdvanceClock(time_delta_us);
    if (i > 2)
      EXPECT_EQ(estimator_->GetJitterEstimate(0), 0);
    gen.Advance();
  }
}

// Add lots of noise jitter but set upper bound to 25ms.
TEST_F(TestVCMJitterEstimator, TestUpperBound) {
  const int kUpperBoundMs = 42;

  // Set up field trial and reset jitter estimator.
  char string_buf[64];
  rtc::SimpleStringBuilder ssb(string_buf);
  ssb << JitterUpperBoundExperiment::kJitterUpperBoundExperimentName
      << "/Enabled-" << kUpperBoundMs << "/";
  test::ScopedFieldTrials field_trials(ssb.str());
  SetUp();

  ValueGenerator gen(1000);
  uint64_t time_delta_us = rtc::kNumMicrosecsPerSec / 30;
  for (int i = 0; i < 60; ++i) {
    estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
    AdvanceClock(time_delta_us);
    EXPECT_LE(estimator_->GetJitterEstimate(25), kUpperBoundMs);
    gen.Advance();
  }
}

}  // namespace webrtc
