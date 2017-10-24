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

#include <string>

#include "media/engine/vp8_encoder_proxy.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace testing {

class FakeEncoder : public VideoEncoder {
 public:
  explicit FakeEncoder(bool supports_simulcast)
      : supports_simulcast_(supports_simulcast) {}
  virtual ~FakeEncoder() {}

  int32_t InitEncode(const VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override {
    if (codec_settings->numberOfSimulcastStreams > 1 && !supports_simulcast_) {
      return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
    } else {
      return WEBRTC_VIDEO_CODEC_OK;
    }
  }

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  int32_t Encode(const VideoFrame& frame,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t SetRates(uint32_t bitrate, uint32_t framerate) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  const char* ImplementationName() const override { return "Fake"; }

 private:
  bool supports_simulcast_;
};

class FakeWebRtcVideoEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  explicit FakeWebRtcVideoEncoderFactory(bool supports_simulcast)
      : supports_simulcast_(supports_simulcast) {}
  virtual ~FakeWebRtcVideoEncoderFactory() {}

  webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec) override {
    return new FakeEncoder(supports_simulcast_);
  }

  const std::vector<cricket::VideoCodec>& supported_codecs() const override {
    return supported_codecs_;
  }

  bool EncoderTypeHasInternalSource(
      webrtc::VideoCodecType type) const override {
    return false;
  }

  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override {}

 private:
  std::vector<cricket::VideoCodec> supported_codecs_;
  bool supports_simulcast_;
};

TEST(VP8EncoderProxy, ChoosesCorrectImplementation) {
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

  FakeWebRtcVideoEncoderFactory simulcast_factory(true);
  VP8EncoderProxy simulcast_enabled_proxy(&simulcast_factory);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, 4, 1200));
  EXPECT_EQ(kSimulcastEnabledImplementation,
            simulcast_enabled_proxy.ImplementationName());
  simulcast_enabled_proxy.Release();

  FakeWebRtcVideoEncoderFactory nonsimulcast_factory(false);
  VP8EncoderProxy simulcast_disabled_proxy(&nonsimulcast_factory);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_disabled_proxy.InitEncode(&codec_settings, 4, 1200));
  EXPECT_EQ(kSimulcastDisabledImplementation,
            simulcast_disabled_proxy.ImplementationName());
  simulcast_disabled_proxy.Release();
}

}  // namespace testing
}  // namespace webrtc
