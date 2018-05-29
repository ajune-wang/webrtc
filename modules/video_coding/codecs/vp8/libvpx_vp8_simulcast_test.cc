/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp8/simulcast_test_fixture_impl.h"
#include "rtc_base/ptr_util.h"
#include "test/function_video_decoder_factory.h"
#include "test/function_video_encoder_factory.h"

namespace webrtc {
namespace test {

namespace {
std::unique_ptr<SimulcastTestFixture> CreateSimulcastTestFixture() {
  std::unique_ptr<VideoEncoderFactory> encoder_factory =
      rtc::MakeUnique<FunctionVideoEncoderFactory>(
          []() { return VP8Encoder::Create(); });
  std::unique_ptr<VideoDecoderFactory> decoder_factory =
      rtc::MakeUnique<FunctionVideoDecoderFactory>(
          []() { return VP8Decoder::Create(); });
  return rtc::MakeUnique<SimulcastTestFixtureImpl>(std::move(encoder_factory),
                                                   std::move(decoder_factory));
}
}  // namespace

TEST(LibvpxVp8SimulcastTest, TestKeyFrameRequestsOnAllStreams) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestKeyFrameRequestsOnAllStreams();
}

TEST(LibvpxVp8SimulcastTest, TestPaddingAllStreams) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestPaddingAllStreams();
}

TEST(LibvpxVp8SimulcastTest, TestPaddingTwoStreams) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestPaddingTwoStreams();
}

TEST(LibvpxVp8SimulcastTest, TestPaddingTwoStreamsOneMaxedOut) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestPaddingTwoStreamsOneMaxedOut();
}

TEST(LibvpxVp8SimulcastTest, TestPaddingOneStream) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestPaddingOneStream();
}

TEST(LibvpxVp8SimulcastTest, TestPaddingOneStreamTwoMaxedOut) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestPaddingOneStreamTwoMaxedOut();
}

TEST(LibvpxVp8SimulcastTest, TestSendAllStreams) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestSendAllStreams();
}

TEST(LibvpxVp8SimulcastTest, TestDisablingStreams) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestDisablingStreams();
}

TEST(LibvpxVp8SimulcastTest, TestActiveStreams) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestActiveStreams();
}

TEST(LibvpxVp8SimulcastTest, TestSwitchingToOneStream) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestSwitchingToOneStream();
}

TEST(LibvpxVp8SimulcastTest, TestSwitchingToOneOddStream) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestSwitchingToOneOddStream();
}

TEST(LibvpxVp8SimulcastTest, TestSwitchingToOneSmallStream) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestSwitchingToOneSmallStream();
}

TEST(LibvpxVp8SimulcastTest, TestSpatioTemporalLayers333PatternEncoder) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestSpatioTemporalLayers333PatternEncoder();
}

TEST(LibvpxVp8SimulcastTest, TestStrideEncodeDecode) {
  auto fixture = CreateSimulcastTestFixture();
  fixture->TestStrideEncodeDecode();
}

}  // namespace test
}  // namespace webrtc
