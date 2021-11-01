/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/zero_hertz_encoder_adapter.h"

#include <utility>
#include <vector>

#include "api/video/nv12_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "rtc_base/ref_counted_object.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Mock;
using ::testing::Ref;

VideoFrame CreateFrame(int64_t ntp_time_ms) {
  auto buffer = rtc::make_ref_counted<NV12Buffer>(/*width=*/16, /*height=*/16);
  return VideoFrame::Builder()
      .set_video_frame_buffer(std::move(buffer))
      .set_ntp_time_ms(ntp_time_ms)
      .set_timestamp_ms(ntp_time_ms)
      .set_rotation(kVideoRotation_0)
      .build();
}

class MockCallback : public ZeroHertzEncoderAdapterInterface::Callback {
 public:
  MOCK_METHOD(void, OnZeroHertzModeDeactivated, (), (override));
};

class MockSink : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  MOCK_METHOD(void, OnFrame, (const VideoFrame&), (override));
};

class ZeroHertzFieldTrialEnabler : public test::ScopedFieldTrials {
 public:
  ZeroHertzFieldTrialEnabler()
      : test::ScopedFieldTrials("WebRTC-ZeroHertzScreenshare/Enabled/") {}
};

TEST(ZeroHertzEncoderAdapterTest, ForwardsFramesOnConstruction) {
  MockCallback callback;
  MockSink sink;
  VideoFrame frame = CreateFrame(0);
  auto adapter = ZeroHertzEncoderAdapterInterface::Create();
  adapter->Initialize(sink, callback);
  EXPECT_CALL(sink, OnFrame(Ref(frame))).Times(1);
  adapter->OnFrame(frame);
  Mock::VerifyAndClearExpectations(&sink);
  EXPECT_CALL(sink, OnFrame(Ref(frame))).Times(1);
  adapter->OnFrame(frame);
  Mock::VerifyAndClearExpectations(&sink);
}

TEST(ZeroHertzEncoderAdapterTest, ForwardsFramesOnConstructionUnderFieldTrial) {
  MockCallback callback;
  MockSink sink;
  VideoFrame frame = CreateFrame(0);
  ZeroHertzFieldTrialEnabler enabler;
  auto adapter = ZeroHertzEncoderAdapterInterface::Create();
  adapter->Initialize(sink, callback);
  EXPECT_CALL(sink, OnFrame(Ref(frame))).Times(1);
  adapter->OnFrame(frame);
  Mock::VerifyAndClearExpectations(&sink);
  EXPECT_CALL(sink, OnFrame(Ref(frame))).Times(1);
  adapter->OnFrame(frame);
  Mock::VerifyAndClearExpectations(&sink);
}

TEST(ZeroHertzEncoderAdapterTest, IsDisabledOnConstruction1) {
  // Checks that the adapter is disabled after construction by not receiving
  // OnZeroHertzModeDeactivated when disabling content type, and then
  // constraints.
  MockCallback callback;
  MockSink sink;
  VideoFrame frame = CreateFrame(0);
  auto adapter = ZeroHertzEncoderAdapterInterface::Create();
  adapter->Initialize(sink, callback);

  for (const auto& deactivate_fn : std::vector<std::function<void()>>{
           [&adapter] { adapter->SetEnabledByContentType(false); },
           [&adapter] { adapter->SetEnabledByConstraints(false); }}) {
    deactivate_fn();
    adapter->OnFrame(frame);
    EXPECT_CALL(callback, OnZeroHertzModeDeactivated).Times(0);
    Mock::VerifyAndClearExpectations(&callback);
  }
}

TEST(ZeroHertzEncoderAdapterTest, IsDisabledOnConstruction2) {
  // Checks that the adapter is disabled after construction by not receiving
  // OnZeroHertzModeDeactivated when disabling by constraints, and then by
  // content type (i.e. the opposite sequence of DisabledOnConstruction1).
  MockCallback callback;
  MockSink sink;
  VideoFrame frame = CreateFrame(0);
  auto adapter = ZeroHertzEncoderAdapterInterface::Create();
  adapter->Initialize(sink, callback);

  for (const auto& deactivate_fn : std::vector<std::function<void()>>{
           [&adapter] { adapter->SetEnabledByConstraints(false); },
           [&adapter] { adapter->SetEnabledByContentType(false); }}) {
    deactivate_fn();
    adapter->OnFrame(frame);
    EXPECT_CALL(callback, OnZeroHertzModeDeactivated).Times(0);
    Mock::VerifyAndClearExpectations(&callback);
  }
}

TEST(ZeroHertzEncoderAdapterTest,
     ForwardsFramesWhenEnabledWhenNotUnderFieldTrial) {
  MockCallback callback;
  MockSink sink;
  VideoFrame frame = CreateFrame(0);
  auto adapter = ZeroHertzEncoderAdapterInterface::Create();
  adapter->Initialize(sink, callback);

  // Activate the adapter-> We should be transporting frames.
  adapter->SetEnabledByConstraints(true);
  adapter->SetEnabledByContentType(true);
  EXPECT_CALL(sink, OnFrame(Ref(frame)));
  adapter->OnFrame(frame);
  Mock::VerifyAndClearExpectations(&sink);
}

TEST(ZeroHertzEncoderAdapterTest, IsDisabledWhenNotUnderFieldTrial) {
  MockCallback callback;
  MockSink sink;
  auto adapter = ZeroHertzEncoderAdapterInterface::Create();
  adapter->Initialize(sink, callback);

  // Perform a sequence that should activate the adapter->
  adapter->SetEnabledByConstraints(true);
  adapter->SetEnabledByContentType(true);

  // Deactivate the adapter-> We should not get disable callbacks when
  // transporting frames.
  for (const auto& deactivate_fn : std::vector<std::function<void()>>{
           [&adapter] { adapter->SetEnabledByContentType(false); },
           [&adapter] { adapter->SetEnabledByConstraints(false); }}) {
    deactivate_fn();
    EXPECT_CALL(callback, OnZeroHertzModeDeactivated).Times(0);
    adapter->OnFrame(CreateFrame(0));
    Mock::VerifyAndClearExpectations(&sink);
    Mock::VerifyAndClearExpectations(&callback);

    // Re-enable.
    adapter->SetEnabledByConstraints(true);
    adapter->SetEnabledByContentType(true);
  }
}

TEST(ZeroHertzEncoderAdapterTest, IsEnabledWhenActivatedUnderFieldTrial) {
  MockCallback callback;
  MockSink sink;
  ZeroHertzFieldTrialEnabler enabler;
  auto adapter = ZeroHertzEncoderAdapterInterface::Create();
  adapter->Initialize(sink, callback);

  // Activate the adapter-> We should be transporting frames.
  adapter->SetEnabledByConstraints(true);
  adapter->SetEnabledByContentType(true);

  // Deactivate the adapter-> We should get disable callbacks when transporting
  // frames.
  for (const auto& deactivate_fn : std::vector<std::function<void()>>{
           [&adapter] { adapter->SetEnabledByContentType(false); },
           [&adapter] { adapter->SetEnabledByConstraints(false); }}) {
    deactivate_fn();
    EXPECT_CALL(callback, OnZeroHertzModeDeactivated);
    adapter->OnFrame(CreateFrame(0));
    Mock::VerifyAndClearExpectations(&callback);

    // Re-enable.
    adapter->SetEnabledByConstraints(true);
    adapter->SetEnabledByContentType(true);
  }
}

}  // namespace
}  // namespace webrtc
