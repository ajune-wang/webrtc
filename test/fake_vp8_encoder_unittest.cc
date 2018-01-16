/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/video_coding/codecs/vp8/simulcast_test_utility.h"
#include "rtc_base/ptr_util.h"
#include "test/fake_decoder.h"
#include "test/fake_vp8_encoder.h"

namespace webrtc {
namespace test {

using webrtc::testing::TestVp8Simulcast;

class TestFakeVp8Codec : public TestVp8Simulcast {
 protected:
  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return rtc::MakeUnique<FakeVP8Encoder>(Clock::GetRealTimeClock());
  }
  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return rtc::MakeUnique<FakeDecoder>();
  }
};

TEST_F(TestFakeVp8Codec, TestKeyFrameRequestsOnAllStreams) {
  TestVp8Simulcast::TestKeyFrameRequestsOnAllStreams();
}

TEST_F(TestFakeVp8Codec, TestPaddingAllStreams) {
  TestVp8Simulcast::TestPaddingAllStreams();
}

TEST_F(TestFakeVp8Codec, TestPaddingTwoStreams) {
  TestVp8Simulcast::TestPaddingTwoStreams();
}

TEST_F(TestFakeVp8Codec, TestPaddingTwoStreamsOneMaxedOut) {
  TestVp8Simulcast::TestPaddingTwoStreamsOneMaxedOut();
}

TEST_F(TestFakeVp8Codec, TestPaddingOneStream) {
  TestVp8Simulcast::TestPaddingOneStream();
}

TEST_F(TestFakeVp8Codec, TestPaddingOneStreamTwoMaxedOut) {
  TestVp8Simulcast::TestPaddingOneStreamTwoMaxedOut();
}

TEST_F(TestFakeVp8Codec, TestSendAllStreams) {
  TestVp8Simulcast::TestSendAllStreams();
}

TEST_F(TestFakeVp8Codec, TestDisablingStreams) {
  TestVp8Simulcast::TestDisablingStreams();
}

TEST_F(TestFakeVp8Codec, TestSwitchingToOneStream) {
  TestVp8Simulcast::TestSwitchingToOneStream();
}

TEST_F(TestFakeVp8Codec, TestSwitchingToOneOddStream) {
  TestVp8Simulcast::TestSwitchingToOneOddStream();
}

TEST_F(TestFakeVp8Codec, TestSwitchingToOneSmallStream) {
  TestVp8Simulcast::TestSwitchingToOneSmallStream();
}

TEST_F(TestFakeVp8Codec, TestSaptioTemporalLayers333PatternEncoder) {
  TestVp8Simulcast::TestSaptioTemporalLayers333PatternEncoder();
}

}  // namespace test
}  // namespace webrtc
