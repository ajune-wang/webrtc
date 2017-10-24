/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "media/engine/vp8_encoder_simulcast_proxy.h"

#include <string>

#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace testing {

class MockEncoder : public VideoEncoder {
 public:
  // TODO(nisse): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  explicit MockEncoder(bool supports_simulcast)
      : supports_simulcast_(supports_simulcast) {}
  virtual ~MockEncoder() {}

  int32_t InitEncode(const VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) /*override*/ {
    if (codec_settings->numberOfSimulcastStreams > 1 && !supports_simulcast_) {
      return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
    } else {
      return WEBRTC_VIDEO_CODEC_OK;
    }
  }

  MOCK_METHOD1(RegisterEncodeCompleteCallback, int32_t(EncodedImageCallback*));

  MOCK_METHOD0(Release, int32_t());

  MOCK_METHOD3(
      Encode,
      int32_t(const VideoFrame& inputImage,
              const CodecSpecificInfo* codecSpecificInfo,
              const std::vector<FrameType>* frame_types) /* override */);

  MOCK_METHOD2(SetChannelParameters, int32_t(uint32_t packetLoss, int64_t rtt));

  const char* ImplementationName() const /*override*/ { return "Fake"; }

 private:
  bool supports_simulcast_;
};

class MockWebRtcVideoEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  explicit MockWebRtcVideoEncoderFactory(bool supports_simulcast)
      : supports_simulcast_(supports_simulcast) {}
  virtual ~MockWebRtcVideoEncoderFactory() {}

  webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec) /*override*/ {
    return new MockEncoder(supports_simulcast_);
  }

  MOCK_CONST_METHOD0(supported_codecs, std::vector<cricket::VideoCodec>&());

  MOCK_METHOD1(DestroyVideoEncoder, void(webrtc::VideoEncoder*));

 private:
  bool supports_simulcast_;
};

TEST(VP8EncoderSimulcastProxy, ChoosesCorrectImplementation) {
  const std::string kSimulcastEnabledImplementation = "Fake";
  const std::string kSimulcastDisabledImplementation =
      "SimulcastEncoderAdapter (Fake, Fake, Fake)";
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  codec_settings.simulcastStream[0] = {
      test::kTestWidth, test::kTestHeight, 2, 2000, 1000, 1000, 56};
  codec_settings.simulcastStream[1] = {
      test::kTestWidth, test::kTestHeight, 2, 3000, 1000, 1000, 56};
  codec_settings.simulcastStream[2] = {
      test::kTestWidth, test::kTestHeight, 2, 5000, 1000, 1000, 56};
  codec_settings.numberOfSimulcastStreams = 3;

  MockWebRtcVideoEncoderFactory simulcast_factory(true);
  VP8EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, 4, 1200));
  EXPECT_EQ(kSimulcastEnabledImplementation,
            simulcast_enabled_proxy.ImplementationName());
  simulcast_enabled_proxy.Release();

  MockWebRtcVideoEncoderFactory nonsimulcast_factory(false);
  VP8EncoderSimulcastProxy simulcast_disabled_proxy(&nonsimulcast_factory);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_disabled_proxy.InitEncode(&codec_settings, 4, 1200));
  EXPECT_EQ(kSimulcastDisabledImplementation,
            simulcast_disabled_proxy.ImplementationName());
  simulcast_disabled_proxy.Release();
}

}  // namespace testing
}  // namespace webrtc
