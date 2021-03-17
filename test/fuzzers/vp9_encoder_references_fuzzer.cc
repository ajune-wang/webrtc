/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/webrtc_key_value_config.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/interface/mock_libvpx_interface.h"
#include "modules/video_coding/codecs/vp9/libvpx_vp9_encoder.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {

using test::FuzzDataHelper;
using ::testing::_;
using ::testing::A;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

class FrameValidator : public EncodedImageCallback {
 public:
  ~FrameValidator() override = default;
  Result OnEncodedImage(const EncodedImage& /*encoded_image*/,
                        const CodecSpecificInfo* codec_specific_info) override {
    RTC_CHECK(codec_specific_info);
    // TODO(danilchap): Validate references are consistent with previously seen
    // frames.
    frames_.push_back(*codec_specific_info);
    return Result(Result::OK);
  }

 private:
  std::vector<CodecSpecificInfo> frames_;
};

class FieldTrials : public WebRtcKeyValueConfig {
 public:
  explicit FieldTrials(FuzzDataHelper& config)
      : flags_(config.ReadOrDefaultValue<uint8_t>(0)) {}

  ~FieldTrials() override = default;
  std::string Lookup(absl::string_view key) const override {
    static constexpr absl::string_view kBinaryFieldTrials[] = {
        "WebRTC-Vp9DependencyDescriptor",
        "WebRTC-Vp9ExternalRefCtrl",
        "WebRTC-Vp9IssueKeyFrameOnLayerDeactivation",
        "WebRTC-VP9QualityScaler",
    };
    for (size_t i = 0; i < ABSL_ARRAYSIZE(kBinaryFieldTrials); ++i) {
      if (key == kBinaryFieldTrials[i]) {
        return (flags_ & (1u << i)) ? "Enabled" : "Disabled";
      }
    }

    // Consciously(?) ignore following field trials.
    if (key == "WebRTC-CongestionWindow" ||
        key == "WebRTC-UseBaseHeavyVP8TL3RateAllocation" ||
        key == "WebRTC-SimulcastUpswitchHysteresisPercent" ||
        key == "WebRTC-SimulcastScreenshareUpswitchHysteresisPercent" ||
        key == "WebRTC-VideoRateControl" ||
        key == "WebRTC-VP9-PerformanceFlags" ||
        key == "WebRTC-VP9VariableFramerateScreenshare") {
      return "";
    }
    // Crash when using unexpected field trial to decide if it should be fuzzed
    // or have a constant value.
    RTC_CHECK(false) << "Unfuzzed field trial " << key << "\n";
  }

 private:
  const uint8_t flags_;
};

VideoCodec CodecSettings(FuzzDataHelper& rng) {
  VideoCodec codec_settings = {};
  codec_settings.codecType = kVideoCodecVP9;
  codec_settings.maxFramerate = 30;
  rng.CopyTo(&codec_settings.width);
  rng.CopyTo(&codec_settings.height);
  uint8_t config = rng.ReadOrDefaultValue<uint8_t>(0);
  // Test up to to 4 spatial and 4 temporal layers.
  codec_settings.VP9()->numberOfSpatialLayers =
      1 + ((config & 0b1100'0000) >> 6);
  codec_settings.VP9()->numberOfTemporalLayers =
      1 + ((config & 0b0011'0000) >> 4);
  codec_settings.VP9()->interLayerPred =
      static_cast<InterLayerPredMode>((config & 0b11'00) >> 2);
  codec_settings.VP9()->flexibleMode = (config & 0b01) != 0;
  codec_settings.VP9()->frameDroppingOn = (config & 0b10) != 0;
  return codec_settings;
}

VideoEncoder::Settings EncoderSettings() {
  return VideoEncoder::Settings(VideoEncoder::Capabilities(false),
                                /*number_of_cores=*/1,
                                /*max_payload_size=*/0);
}

enum Actions {
  kStartEncode = 0b00,
  kEncodeCallback = 0b01,
  // TODO(danilchap): Add action to SetRates, i.e. to enable/disable layers.
};

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  FuzzDataHelper helper(rtc::MakeArrayView(data, size));

  FrameValidator validator;
  FieldTrials field_trials(helper);
  // TODO(danilchap): decide between NiceMock ignoring lot's of control messages
  // and StrictMock to catch all of them and decide how to fake them.
  auto libvpx = std::make_unique<NiceMock<MockLibvpxInterface>>();
  // Setup call callbacks for the fake
  vpx_codec_priv_output_cx_pkt_cb_pair_t callback = {};
  vpx_image_t img = {};

  ON_CALL(*libvpx, codec_control(_, VP9E_REGISTER_CX_CALLBACK, A<void*>()))
      .WillByDefault(WithArg<2>([&](void* cbp) {
        callback =
            *reinterpret_cast<vpx_codec_priv_output_cx_pkt_cb_pair_t*>(cbp);
        return VPX_CODEC_OK;
      }));
  ON_CALL(*libvpx, img_wrap).WillByDefault(Return(&img));

  // Initialize encoder
  LibvpxVp9Encoder encoder(cricket::VideoCodec(), std::move(libvpx),
                           field_trials);
  VideoCodec codec = CodecSettings(helper);
  if (encoder.InitEncode(&codec, EncoderSettings()) != WEBRTC_VIDEO_CODEC_OK) {
    return;
  }
  RTC_CHECK_EQ(encoder.RegisterEncodeCompleteCallback(&validator),
               WEBRTC_VIDEO_CODEC_OK);

  std::vector<VideoFrameType> frame_types(1);
  VideoFrame fake_image = VideoFrame::Builder()
                              .set_video_frame_buffer(I420Buffer::Create(1, 1))
                              .build();

  // Start producing frames at random.
  while (helper.CanReadBytes(1)) {
    uint8_t action = helper.Read<uint8_t>();
    switch (action & 0b11) {
      case kStartEncode:
        frame_types[0] = (action & 0b100) ? VideoFrameType::kVideoFrameKey
                                          : VideoFrameType::kVideoFrameDelta;
        encoder.Encode(fake_image, &frame_types);
        break;
      case kEncodeCallback:
        vpx_codec_cx_pkt pkt;
        // TODO(danilchap): Set up libvpx callbacks for layer_id and references
        // based on latest call to encode.
        callback.output_cx_pkt(&pkt, callback.user_priv);
        break;
      default:
        // Unspecificed values are noop.
        break;
    }
  }
}
}  // namespace webrtc
