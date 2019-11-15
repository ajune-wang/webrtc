/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_smoothing_inhibitor.h"

#include "api/video/i420_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::Eq;

class MockVideoSink : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  MOCK_METHOD1(OnFrame, void(const VideoFrame&));
};

class FrameSmoothingInhibitorTest : public ::testing::Test {
 public:
  void OnFrame(FrameSmoothingInhibitor* inhibitor) {
    inhibitor->OnFrame(
        VideoFrame::Builder()
            .set_video_frame_buffer(I420Buffer::Create(10, 10, 10, 14, 90))
            .build());
  }
};

TEST_F(FrameSmoothingInhibitorTest, ForwardsToSmootherAfterConstruction) {
  MockVideoSink smoothing_sink;
  MockVideoSink direct_sink;
  EXPECT_CALL(smoothing_sink, OnFrame).Times(2);
  EXPECT_CALL(direct_sink, OnFrame).Times(0);
  FrameSmoothingInhibitor inhibitor(&smoothing_sink, &direct_sink);
  OnFrame(&inhibitor);
  OnFrame(&inhibitor);
}

TEST_F(FrameSmoothingInhibitorTest, SelectsDirectRouteAsDisabled) {
  MockVideoSink smoothing_sink;
  MockVideoSink direct_sink;
  EXPECT_CALL(smoothing_sink, OnFrame).Times(0);
  EXPECT_CALL(direct_sink, OnFrame).Times(1);
  FrameSmoothingInhibitor inhibitor(&smoothing_sink, &direct_sink);
  inhibitor.SetSmoothingEnabled(false);
  OnFrame(&inhibitor);
  ::testing::Mock::VerifyAndClearExpectations(&smoothing_sink);
  ::testing::Mock::VerifyAndClearExpectations(&direct_sink);
  inhibitor.SetSmoothingEnabled(true);
  EXPECT_CALL(smoothing_sink, OnFrame).Times(1);
  EXPECT_CALL(direct_sink, OnFrame).Times(0);
  OnFrame(&inhibitor);
}

}  // namespace webrtc
