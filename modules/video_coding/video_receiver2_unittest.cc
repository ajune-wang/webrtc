/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/video_receiver2.h"

#include <memory>
#include <utility>

#include "api/test/mock_video_decoder.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/decoder_database.h"
#include "modules/video_coding/timing/timing.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {

class MockVCMReceiveCallback : public VCMReceiveCallback {
 public:
  MockVCMReceiveCallback() {}
  ~MockVCMReceiveCallback() override {}

  MOCK_METHOD(
      int32_t,
      FrameToRender,
      (VideoFrame&, absl::optional<uint8_t>, TimeDelta, VideoContentType),
      (override));
  MOCK_METHOD(void, OnIncomingPayloadType, (int), (override));
  MOCK_METHOD(void, OnDecoderImplementationName, (const char*), (override));
};

TEST(VideoReceiver2, RegisterExternalDecoder) {
  test::ScopedKeyValueConfig field_trials;
  SimulatedClock clock(Timestamp::Millis(1337));
  VCMTiming timing(&clock, field_trials);
  testing::StrictMock<MockVCMReceiveCallback> receive_callback;

  VideoReceiver2 receiver(&clock, &timing, field_trials);
  receiver.RegisterReceiveCallback(&receive_callback);

  constexpr int kPayloadType = 1;
  ASSERT_FALSE(receiver.IsExternalDecoderRegistered(kPayloadType));

  // Register a decoder, check for correctness, then unregister and check again.
  auto decoder = std::make_unique<testing::StrictMock<MockVideoDecoder>>();
  receiver.RegisterExternalDecoder(std::move(decoder), kPayloadType);
  EXPECT_TRUE(receiver.IsExternalDecoderRegistered(kPayloadType));
  receiver.RegisterExternalDecoder(nullptr, kPayloadType);
  EXPECT_FALSE(receiver.IsExternalDecoderRegistered(kPayloadType));
}

}  // namespace
}  // namespace webrtc
