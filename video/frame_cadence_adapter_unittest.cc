/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_cadence_adapter.h"

#include <utility>
#include <vector>

#include "api/video/nv12_buffer.h"
#include "api/video/video_frame.h"
#include "api/video_track_source_constraints.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/ref_counted_object.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Pair;
using ::testing::Ref;
using ::testing::UnorderedElementsAre;

VideoFrame CreateFrame() {
  return VideoFrame::Builder()
      .set_video_frame_buffer(
          rtc::make_ref_counted<NV12Buffer>(/*width=*/16, /*height=*/16))
      .build();
}

class MockCallback : public FrameCadenceAdapterInterface::Callback {
 public:
  MOCK_METHOD(void,
              OnFrame,
              (const VideoFrame&, absl::optional<TimeDelta>),
              (override));
  MOCK_METHOD(void, OnDiscardedFrame, (), (override));
};

class ZeroHertzFieldTrialDisabler : public test::ScopedFieldTrials {
 public:
  ZeroHertzFieldTrialDisabler()
      : test::ScopedFieldTrials("WebRTC-ZeroHertzScreenshare/Disabled/") {}
};

class ZeroHertzFieldTrialEnabler : public test::ScopedFieldTrials {
 public:
  ZeroHertzFieldTrialEnabler()
      : test::ScopedFieldTrials("WebRTC-ZeroHertzScreenshare/Enabled/") {}
};

TEST(FrameCadenceAdapterTest,
     ForwardsFramesOnConstructionAndUnderDisabledFieldTrial) {
  auto disabler = std::make_unique<ZeroHertzFieldTrialDisabler>();
  for (int i = 0; i != 2; i++) {
    MockCallback callback;
    auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
    adapter->Initialize(&callback);
    VideoFrame frame = CreateFrame();
    EXPECT_CALL(callback, OnFrame(Ref(frame), Eq(absl::nullopt))).Times(1);
    adapter->OnFrame(frame);
    Mock::VerifyAndClearExpectations(&callback);
    EXPECT_CALL(callback, OnDiscardedFrame).Times(1);
    adapter->OnDiscardedFrame();
    Mock::VerifyAndClearExpectations(&callback);

    disabler = nullptr;
  }
}

TEST(FrameCadenceAdapterTest, FrameRateFollowsRateStatisticsWhenInactivated) {
  auto disabler = std::make_unique<ZeroHertzFieldTrialDisabler>();
  for (int i = 0; i != 2; i++) {
    GlobalSimulatedTimeController time_controller(Timestamp::Millis(0));
    auto adapter =
        FrameCadenceAdapterInterface::Create(time_controller.GetClock());

    // Create an "oracle" rate statistics which should be followed on a sequence
    // of frames.
    RateStatistics rate(
        FrameCadenceAdapterInterface::kFrameRateAvergingWindowSizeMs, 1000);

    for (int frame = 0; frame != 10; frame++) {
      time_controller.AdvanceTime(TimeDelta::Millis(10));
      rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
      adapter->UpdateFrameRate();
      EXPECT_EQ(rate.Rate(time_controller.GetClock()->TimeInMilliseconds()),
                adapter->GetInputFramerateFps())
          << " failed for frame " << frame;
    }
    disabler = nullptr;
  }
}

TEST(FrameCadenceAdapterTest, FrameRateFollowsMaxFpsWhenZeroHertzActivated) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(0));
  auto adapter =
      FrameCadenceAdapterInterface::Create(time_controller.GetClock());
  adapter->SetEnabledByContentType(true);
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});
  for (int frame = 0; frame != 10; frame++) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    adapter->UpdateFrameRate();
    EXPECT_EQ(adapter->GetInputFramerateFps(), 1u);
  }
}

class FrameCadenceAdapterMetricsTest : public ::testing::Test {
 public:
  FrameCadenceAdapterMetricsTest() { metrics::Reset(); }
};

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoUmasWithNoFrameTransfer) {
  MockCallback callback;
  auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
  adapter->Initialize(&callback);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{absl::nullopt, 1});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{2, 3});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4, 4});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5, absl::nullopt});
  EXPECT_TRUE(metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoUmasWithoutEnabledContentType) {
  MockCallback callback;
  auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
  adapter->Initialize(&callback);
  adapter->OnFrame(CreateFrame());
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{absl::nullopt, 1});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{2, 3});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4, 4});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5, absl::nullopt});
  EXPECT_TRUE(metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoConstraintsIfUnsetOnFrame) {
  MockCallback callback;
  auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
  adapter->Initialize(&callback);
  adapter->SetEnabledByContentType(true);
  adapter->OnFrame(CreateFrame());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists"),
      ElementsAre(Pair(false, 1)));
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsEmptyConstraintsIfSetOnFrame) {
  MockCallback callback;
  auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
  adapter->Initialize(&callback);
  adapter->SetEnabledByContentType(true);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnFrame(CreateFrame());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
  adapter->Initialize(&callback);
  adapter->SetEnabledByContentType(true);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, 2.0});
  adapter->OnFrame(CreateFrame());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value"),
      ElementsAre(Pair(2.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max"),
      ElementsAre(Pair(2.0, 1)));
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinConstraintIfSetOnFrame) {
  MockCallback callback;
  auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
  adapter->Initialize(&callback);
  adapter->SetEnabledByContentType(true);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{3.0, absl::nullopt});
  adapter->OnFrame(CreateFrame());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value"),
      ElementsAre(Pair(3.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinGtMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
  adapter->Initialize(&callback);
  adapter->SetEnabledByContentType(true);
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5.0, 4.0});
  adapter->OnFrame(CreateFrame());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value"),
      ElementsAre(Pair(5.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value"),
      ElementsAre(Pair(4.0, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_THAT(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne"),
      ElementsAre(Pair(60 * 5.0 + 4.0 - 1, 1)));
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinLtMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  auto adapter = FrameCadenceAdapterInterface::Create(nullptr);
  adapter->Initialize(&callback);
  adapter->SetEnabledByContentType(true);
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4.0, 5.0});
  adapter->OnFrame(CreateFrame());
  EXPECT_THAT(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min"),
              ElementsAre(Pair(4.0, 1)));
  EXPECT_THAT(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max"),
              ElementsAre(Pair(5.0, 1)));
  EXPECT_THAT(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne"),
      ElementsAre(Pair(60 * 4.0 + 5.0 - 1, 1)));
}

}  // namespace
}  // namespace webrtc
