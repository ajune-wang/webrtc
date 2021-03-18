/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#include "absl/base/macros.h"
#include "api/array_view.h"
#include "api/transport/webrtc_key_value_config.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/interface/mock_libvpx_interface.h"
#include "modules/video_coding/codecs/vp9/libvpx_vp9_encoder.h"
#include "rtc_base/numerics/safe_compare.h"
#include "test/fuzzers/fuzz_data_helper.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

using test::FuzzDataHelper;
using ::testing::NiceMock;

class FrameValidator : public EncodedImageCallback {
 public:
  ~FrameValidator() override = default;
  Result OnEncodedImage(const EncodedImage& /*encoded_image*/,
                        const CodecSpecificInfo* codec_specific_info) override {
    RTC_CHECK(codec_specific_info);
    RTC_CHECK_EQ(codec_specific_info->codecType, kVideoCodecVP9);

    // TODO(danilchap): Validate references are consistent with previously seen
    // frames.
    return Result(Result::OK);
  }
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
    };
    for (size_t i = 0; i < ABSL_ARRAYSIZE(kBinaryFieldTrials); ++i) {
      if (key == kBinaryFieldTrials[i]) {
        return (flags_ & (1u << i)) ? "Enabled" : "Disabled";
      }
    }

    // Ignore following field trials.
    if (key == "WebRTC-CongestionWindow" ||
        key == "WebRTC-UseBaseHeavyVP8TL3RateAllocation" ||
        key == "WebRTC-SimulcastUpswitchHysteresisPercent" ||
        key == "WebRTC-SimulcastScreenshareUpswitchHysteresisPercent" ||
        key == "WebRTC-VideoRateControl" ||
        key == "WebRTC-VP9-PerformanceFlags" ||
        key == "WebRTC-VP9VariableFramerateScreenshare" ||
        key == "WebRTC-VP9QualityScaler") {
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
  uint8_t config = rng.ReadOrDefaultValue<uint16_t>(0);
  // Test up to to 4 spatial and 4 temporal layers.
  int num_spatial_layers = 1 + (config & 0b11);
  int num_temporal_layers = 1 + ((config >> 2) & 0b11);
  if (num_spatial_layers > 1) {
    for (int si = 0; si < num_spatial_layers; ++si) {
      SpatialLayer& spatial_layer = codec_settings.spatialLayers[si];
      rng.CopyTo(&spatial_layer.width);
      rng.CopyTo(&spatial_layer.height);
      spatial_layer.maxFramerate = codec_settings.maxFramerate;
      spatial_layer.numberOfTemporalLayers = num_temporal_layers;
    }
  }
  codec_settings.VP9()->numberOfSpatialLayers = num_spatial_layers;
  codec_settings.VP9()->numberOfTemporalLayers = num_temporal_layers;
  codec_settings.VP9()->interLayerPred =
      static_cast<InterLayerPredMode>((config >> 4) & 0b11);
  codec_settings.VP9()->flexibleMode = (config & (1u << 6)) != 0;
  codec_settings.VP9()->frameDroppingOn = (config & (1u << 7)) != 0;
  codec_settings.mode = (config & (1u << 8)) ? VideoCodecMode::kRealtimeVideo
                                             : VideoCodecMode::kScreensharing;
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

struct LibvpxState {
  LibvpxState() {
    pkt.kind = VPX_CODEC_CX_FRAME_PKT;
    pkt.data.frame.buf = pkt_buffer;
    pkt.data.frame.sz = ABSL_ARRAYSIZE(pkt_buffer);
    layer_id.spatial_layer_id = -1;
  }

  int spatial_layer = -1;
  uint8_t pkt_buffer[1000] = {};
  vpx_codec_enc_cfg_t config = {};
  vpx_codec_priv_output_cx_pkt_cb_pair_t callback = {};
  vpx_image_t img = {};
  vpx_svc_ref_frame_config_t ref_config = {};
  vpx_svc_layer_id_t layer_id = {};
  vpx_codec_cx_pkt pkt = {};
};

class FakeLibvpx : public NiceMock<MockLibvpxInterface> {
 public:
  explicit FakeLibvpx(LibvpxState* state) : state_(state) { RTC_CHECK(state_); }

  vpx_codec_err_t codec_enc_config_default(vpx_codec_iface_t* iface,
                                           vpx_codec_enc_cfg_t* cfg,
                                           unsigned int usage) const override {
    state_->config = *cfg;
    return VPX_CODEC_OK;
  }

  vpx_image_t* img_wrap(vpx_image_t* img,
                        vpx_img_fmt_t fmt,
                        unsigned int d_w,
                        unsigned int d_h,
                        unsigned int stride_align,
                        unsigned char* img_data) const override {
    return &state_->img;
  }

  vpx_codec_err_t codec_encode(vpx_codec_ctx_t* ctx,
                               const vpx_image_t* img,
                               vpx_codec_pts_t pts,
                               uint64_t duration,
                               vpx_enc_frame_flags_t flags,
                               uint64_t deadline) const override {
    if (flags & VPX_EFLAG_FORCE_KF) {
      state_->pkt.data.frame.flags = VPX_FRAME_IS_KEY;
    } else {
      state_->pkt.data.frame.flags = 0;
    }
    state_->pkt.data.frame.duration = duration;
    state_->spatial_layer = state_->layer_id.spatial_layer_id;
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                void* param) const override {
    switch (ctrl_id) {
      case VP9E_REGISTER_CX_CALLBACK:
        state_->callback =
            *reinterpret_cast<vpx_codec_priv_output_cx_pkt_cb_pair_t*>(param);
        break;
      default:
        break;
    }
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_control(
      vpx_codec_ctx_t* ctx,
      vp8e_enc_control_id ctrl_id,
      vpx_svc_ref_frame_config_t* param) const override {
    switch (ctrl_id) {
      case VP9E_SET_SVC_REF_FRAME_CONFIG:
        state_->ref_config = *param;
        break;
      case VP9E_GET_SVC_REF_FRAME_CONFIG:
        *param = state_->ref_config;
        break;
      default:
        break;
    }
    return VPX_CODEC_OK;
  }

  vpx_codec_err_t codec_control(vpx_codec_ctx_t* ctx,
                                vp8e_enc_control_id ctrl_id,
                                vpx_svc_layer_id_t* param) const override {
    switch (ctrl_id) {
      case VP9E_SET_SVC_LAYER_ID:
        state_->layer_id = *param;
        break;
      case VP9E_GET_SVC_LAYER_ID:
        *param = state_->layer_id;
        break;
      default:
        break;
    }
    return VPX_CODEC_OK;
  }

 private:
  LibvpxState* const state_;
};

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  FuzzDataHelper helper(rtc::MakeArrayView(data, size));

  FrameValidator validator;
  FieldTrials field_trials(helper);
  // Setup call callbacks for the fake
  LibvpxState state;

  // Initialize encoder
  LibvpxVp9Encoder encoder(cricket::VideoCodec(),
                           std::make_unique<FakeLibvpx>(&state), field_trials);
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
        if (action & 0b1000) {
          // Make frame a key frame regardlesss if it was requested as one.
          state.pkt.data.frame.flags |= VPX_FRAME_IS_KEY;
        }
        break;
      case kEncodeCallback:
        if (state.spatial_layer < 0 ||
            rtc::SafeGe(state.spatial_layer, state.config.ss_number_layers)) {
          // No layer frame to callback with.
          break;
        }
        state.layer_id.spatial_layer_id = state.spatial_layer;
        state.callback.output_cx_pkt(&state.pkt, state.callback.user_priv);
        state.spatial_layer++;
        break;
      default:
        // Unspecificed values are noop.
        break;
    }
  }
}
}  // namespace webrtc
