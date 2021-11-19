/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_source_sink_controller.h"

#include <limits>

#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;
using testing::Eq;
using testing::Field;

namespace webrtc {

namespace {

constexpr int kIntUnconstrained = std::numeric_limits<int>::max();

class FakeSink : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  ~FakeSink() override = default;
  void OnFrame(const VideoFrame& frame) override {}
  void OnDiscardedFrame() override {}
};

class MockVideoSourceWithVideoFrame
    : public rtc::VideoSourceInterface<VideoFrame> {
 public:
  ~MockVideoSourceWithVideoFrame() override = default;
  MOCK_METHOD(void,
              AddOrUpdateSink,
              (rtc::VideoSinkInterface<VideoFrame>*,
               const rtc::VideoSinkWants&),
              (override));
  MOCK_METHOD(void,
              RemoveSink,
              (rtc::VideoSinkInterface<VideoFrame>*),
              (override));
};

}  // namespace

TEST(VideoSourceSinkControllerTest, UnconstrainedByDefault) {
  SinkWantsCalculator calculator;
  EXPECT_EQ(calculator.restrictions(), VideoSourceRestrictions());
  EXPECT_FALSE(calculator.pixels_per_frame_upper_limit().has_value());
  EXPECT_FALSE(calculator.frame_rate_upper_limit().has_value());
  EXPECT_FALSE(calculator.rotation_applied());
  EXPECT_EQ(calculator.resolution_alignment(), 1);

  auto wants = calculator.ComputeWants();
  EXPECT_FALSE(wants.rotation_applied);
  EXPECT_EQ(wants.max_pixel_count, kIntUnconstrained);
  EXPECT_EQ(wants.target_pixel_count, absl::nullopt);
  EXPECT_EQ(wants.max_framerate_fps, kIntUnconstrained);
  EXPECT_EQ(wants.resolution_alignment, 1);
}

TEST(VideoSourceSinkControllerTest, VideoRestrictionsToSinkWants) {
  SinkWantsCalculator calculator;

  VideoSourceRestrictions restrictions = calculator.restrictions();
  // max_pixels_per_frame() maps to `max_pixel_count`.
  restrictions.set_max_pixels_per_frame(42u);
  // target_pixels_per_frame() maps to `target_pixel_count`.
  restrictions.set_target_pixels_per_frame(200u);
  // max_frame_rate() maps to `max_framerate_fps`.
  restrictions.set_max_frame_rate(30.0);
  calculator.SetRestrictions(restrictions);
  auto wants = calculator.ComputeWants();
  EXPECT_EQ(wants.max_pixel_count, 42);
  EXPECT_EQ(wants.target_pixel_count, 200);
  EXPECT_EQ(wants.max_framerate_fps, 30);

  // pixels_per_frame_upper_limit() caps `max_pixel_count`.
  calculator.SetPixelsPerFrameUpperLimit(24);
  // frame_rate_upper_limit() caps `max_framerate_fps`.
  calculator.SetFrameRateUpperLimit(10.0);
  wants = calculator.ComputeWants();
  EXPECT_EQ(wants.max_pixel_count, 24);
  EXPECT_EQ(wants.max_framerate_fps, 10);
}

TEST(VideoSourceSinkControllerTest, RotationApplied) {
  SinkWantsCalculator calculator;
  calculator.SetRotationApplied(true);
  EXPECT_TRUE(calculator.rotation_applied());
  EXPECT_TRUE(calculator.ComputeWants().rotation_applied);
}

TEST(VideoSourceSinkControllerTest, ResolutionAlignment) {
  SinkWantsCalculator calculator;
  calculator.SetResolutionAlignment(13);
  EXPECT_EQ(calculator.resolution_alignment(), 13);
  EXPECT_EQ(calculator.ComputeWants().resolution_alignment, 13);
}

TEST(VideoSourceSinkControllerTest, AddsSinkOnSetSource) {
  FakeSink sink;
  MockVideoSourceWithVideoFrame source;
  SinkWantsCalculator calculator;
  VideoSourceSinkController controller(calculator, &sink);
  EXPECT_CALL(source, AddOrUpdateSink(&sink, Eq(calculator.ComputeWants())));
  controller.CompleteSetSource(controller.BeginSetSource(&source));
}

TEST(VideoSourceSinkControllerTest, RemovesAddsSinkOnSwitchingSource) {
  FakeSink sink;
  MockVideoSourceWithVideoFrame old_source, new_source;
  SinkWantsCalculator calculator;
  VideoSourceSinkController controller(calculator, &sink);
  controller.CompleteSetSource(controller.BeginSetSource(&old_source));
  EXPECT_CALL(old_source, RemoveSink(&sink));
  EXPECT_CALL(new_source, AddOrUpdateSink);
  controller.CompleteSetSource(controller.BeginSetSource(&new_source));
}

TEST(VideoSourceSinkControllerTest, RemovesSinkOnClearSource) {
  FakeSink sink;
  MockVideoSourceWithVideoFrame source;
  SinkWantsCalculator calculator;
  VideoSourceSinkController controller(calculator, &sink);
  controller.CompleteSetSource(controller.BeginSetSource(&source));
  EXPECT_CALL(source, RemoveSink(&sink));
  controller.ClearSource();
}

TEST(VideoSourceSinkControllerTest, IgnoresStaleSetSource) {
  FakeSink sink;
  MockVideoSourceWithVideoFrame source1, source2;
  SinkWantsCalculator calculator;
  VideoSourceSinkController controller(calculator, &sink);
  EXPECT_CALL(source1, AddOrUpdateSink).Times(0);
  EXPECT_CALL(source2, AddOrUpdateSink);
  auto completion1 = controller.BeginSetSource(&source1);
  auto completion2 = controller.BeginSetSource(&source2);
  controller.CompleteSetSource(completion1);
  controller.CompleteSetSource(completion2);
}

TEST(VideoSourceSinkControllerTest, CommitsNewWants) {
  FakeSink sink;
  MockVideoSourceWithVideoFrame source;
  SinkWantsCalculator calculator;
  VideoSourceSinkController controller(calculator, &sink);
  testing::InSequence s;
  EXPECT_CALL(source, AddOrUpdateSink);
  EXPECT_CALL(source,
              AddOrUpdateSink(
                  _, Field(&rtc::VideoSinkWants::resolution_alignment, 42)));
  controller.CompleteSetSource(controller.BeginSetSource(&source));
  calculator.SetResolutionAlignment(42);
  controller.CommitSinkWants();
}

}  // namespace webrtc
