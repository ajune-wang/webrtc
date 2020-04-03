/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "third_party/libaom/source/libaom/aom/aom_codec.h"
#include "third_party/libaom/source/libaom/aom/aom_encoder.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"

namespace webrtc {
namespace {

// Encoder configuration parameters
constexpr int kRefFrames = 8;
constexpr int kInterRefsPerFrame = 7;
constexpr int kQpMax = 56;
constexpr int kQpMin = 10;
constexpr int kDefaultEncSpeed = 7;  // Use values 6, 7, or 8 for RTC.
constexpr int kUsageProfile = 1;     // 0 = good quality; 1 = real-time.
constexpr int kMinQindex = 58;       // Min qindex threshold for QP scaling.
constexpr int kMaxQindex = 180;      // Max qindex threshold for QP scaling.
constexpr int kBitDepth = 8;
constexpr int kLagInFrames = 0;  // No look ahead.
constexpr int kRtpTicksPerSecond = 90000;
constexpr float kMinimumFrameRate = 1.0;

class LibaomAv1Encoder final : public VideoEncoder {
 public:
  LibaomAv1Encoder();
  ~LibaomAv1Encoder();

  int InitEncode(const VideoCodec* codec_settings,
                 const Settings& settings) override;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* encoded_image_callback) override;

  int32_t Release() override;

  int32_t Encode(const VideoFrame& frame,
                 const std::vector<VideoFrameType>* frame_types) override;

  void SetRates(const RateControlParameters& parameters) override;

  EncoderInfo GetEncoderInfo() const override;

 private:
  int set_layer_pattern(int number_spatial_layers,
                        int number_temporal_layers,
                        int superframe_cnt,
                        aom_svc_layer_id_t* layer_id,
                        aom_svc_ref_frame_config_t* ref_frame_config,
                        int spatial_layer_id,
                        int is_key_frame,
                        int ksvc);

 private:
  bool inited_;
  bool keyframe_required_;
  uint8_t num_spatial_layers_;
  uint8_t num_temporal_layers_;
  int superframe_cnt_;
  VideoCodec encoder_settings_;
  aom_image_t* frame_for_encode_;
  aom_codec_ctx_t ctx_;
  aom_codec_enc_cfg_t cfg_;
  aom_svc_params_t svc_params_;
  EncodedImageCallback* encoded_image_callback_;
};

int32_t VerifyCodecSettings(const VideoCodec& codec_settings) {
  if (codec_settings.width < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings.height < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // maxBitrate == 0 represents an unspecified maxBitRate.
  if (codec_settings.maxBitrate > 0 &&
      codec_settings.minBitrate > codec_settings.maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings.maxBitrate > 0 &&
      codec_settings.startBitrate > codec_settings.maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings.startBitrate < codec_settings.minBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings.maxFramerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

LibaomAv1Encoder::LibaomAv1Encoder()
    : inited_(false),
      keyframe_required_(true),
      frame_for_encode_(nullptr),
      encoded_image_callback_(nullptr) {}

LibaomAv1Encoder::~LibaomAv1Encoder() {
  Release();
}

int LibaomAv1Encoder::InitEncode(const VideoCodec* codec_settings,
                                 const Settings& settings) {
  if (codec_settings == nullptr) {
    RTC_LOG(LS_WARNING) << "No codec settings provided to "
                           "LibaomAv1Encoder.";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (settings.number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inited_) {
    RTC_LOG(LS_WARNING) << "Initing LibaomAv1Encoder without first releasing.";
    Release();
  }
  encoder_settings_ = *codec_settings;

  // Sanity checks for encoder configuration.
  const int32_t result = VerifyCodecSettings(encoder_settings_);
  if (result < 0) {
    RTC_LOG(LS_WARNING) << "Incorrect codec settings provided to "
                           "LibaomAv1Encoder.";
    return result;
  }

  // Initialize encoder configuration structure with default values
  aom_codec_err_t ret =
      aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg_, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on aom_codec_enc_config_default.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Overwrite default config with input encoder settings & RTC-relevant values.
  cfg_.g_w = encoder_settings_.width;
  cfg_.g_h = encoder_settings_.height;
  cfg_.g_threads = settings.number_of_cores;
  cfg_.g_timebase.num = 1;
  cfg_.g_timebase.den = kRtpTicksPerSecond;
  cfg_.rc_target_bitrate = encoder_settings_.maxBitrate;  // kilobits/sec.
  cfg_.g_input_bit_depth = kBitDepth;
  cfg_.kf_mode = AOM_KF_DISABLED;
  cfg_.rc_min_quantizer = kQpMin;
  cfg_.rc_max_quantizer = kQpMax;
  cfg_.g_usage = kUsageProfile;

  // Low-latency settings.
  cfg_.rc_end_usage = AOM_CBR;          // Constant Bit Rate (CBR) mode
  cfg_.g_pass = AOM_RC_ONE_PASS;        // One-pass rate control
  cfg_.g_lag_in_frames = kLagInFrames;  // No look ahead when lag equals 0.

  // Creating a wrapper to the image - setting image data to nullptr. Actual
  // pointer will be set in encode. Setting align to 1, as it is meaningless
  // (actual memory is not allocated).
  frame_for_encode_ =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg_.g_w, cfg_.g_h, 1);

  // Flag options: AOM_CODEC_USE_PSNR and AOM_CODEC_USE_HIGHBITDEPTH
  aom_codec_flags_t flags = 0;

  // Initialize an encoder instance.
  ret = aom_codec_enc_init(&ctx_, aom_codec_av1_cx(), &cfg_, flags);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on aom_codec_enc_init.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  inited_ = true;

  // Set control parameters
  ret = aom_codec_control(&ctx_, AOME_SET_CPUUSED, kDefaultEncSpeed);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_CPUUSED.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_ENABLE_TPL_MODEL, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_ENABLE_TPL_MODEL.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_DELTAQ_MODE, 0);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_DELTAQ_MODE.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ret = aom_codec_control(&ctx_, AV1E_SET_AQ_MODE, 3);
  if (ret != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::EncodeInit returned " << ret
                        << " on control AV1E_SET_AQ_MODE.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // SVC SET UP //

  memset(&svc_params_, 0, sizeof(svc_params_));

  num_spatial_layers_ = codec_settings->AV1().numberOfSpatialLayers;
  RTC_DCHECK_GE(num_spatial_layers_, 0);
  num_temporal_layers_ = codec_settings->AV1().numberOfTemporalLayers;
  // Number of spatial and temporal layers.
  svc_params_.number_spatial_layers = num_spatial_layers_;
  svc_params_.number_temporal_layers = num_temporal_layers_;

  // Set layer bitrate and max/min_qp per layer.
  for (int i = 0; i < num_spatial_layers_ * num_temporal_layers_; ++i) {
    svc_params_.max_quantizers[i] = cfg_.rc_max_quantizer;
    svc_params_.min_quantizers[i] = cfg_.rc_min_quantizer;
  }

  // Temporal framerate factors.
  svc_params_.framerate_factor[0] = 1;
  if (num_temporal_layers_ == 2) {
    svc_params_.framerate_factor[0] = 2;
    svc_params_.framerate_factor[1] = 1;
  } else if (num_temporal_layers_ == 3) {
    svc_params_.framerate_factor[0] = 4;
    svc_params_.framerate_factor[1] = 2;
    svc_params_.framerate_factor[2] = 1;
  }

  // Spatial scale factors.
  for (int i = 0; i < num_spatial_layers_; ++i) {
    svc_params_.scaling_factor_num[i] = 1;
    svc_params_.scaling_factor_den[i] = 1;
  }
  if (num_spatial_layers_ == 2) {
    svc_params_.scaling_factor_num[0] = 1;
    svc_params_.scaling_factor_den[0] = 2;
  } else if (num_spatial_layers_ == 3) {
    svc_params_.scaling_factor_num[0] = 1;
    svc_params_.scaling_factor_den[0] = 4;
    svc_params_.scaling_factor_num[1] = 1;
    svc_params_.scaling_factor_den[1] = 2;
  }

  aom_codec_control(&ctx_, AV1E_SET_SVC_PARAMS, &svc_params_);

  superframe_cnt_ = 0;

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t LibaomAv1Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* encoded_image_callback) {
  encoded_image_callback_ = encoded_image_callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t LibaomAv1Encoder::Release() {
  if (frame_for_encode_ != nullptr) {
    aom_img_free(frame_for_encode_);
    frame_for_encode_ = nullptr;
  }
  if (inited_) {
    if (aom_codec_destroy(&ctx_)) {
      return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    inited_ = false;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

// Layer pattenr configuration.
int LibaomAv1Encoder::set_layer_pattern(
    int number_spatial_layers,
    int number_temporal_layers,
    int superframe_cnt,
    aom_svc_layer_id_t* layer_id,
    aom_svc_ref_frame_config_t* ref_frame_config,
    int spatial_layer_id,
    int is_key_frame,
    int ksvc) {
  int layering_mode = 0;
  layer_id->spatial_layer_id = spatial_layer_id;
  // Set the reference map buffer idx for the 7 references:
  // LAST_FRAME (0), LAST2_FRAME(1), LAST3_FRAME(2), GOLDEN_FRAME(3),
  // BWDREF_FRAME(4), ALTREF2_FRAME(5), ALTREF_FRAME(6).
  for (int i = 0; i < kInterRefsPerFrame; i++)
    ref_frame_config->ref_idx[i] = i;
  for (int i = 0; i < kRefFrames; i++)
    ref_frame_config->refresh[i] = 0;
  // Note only use LAST and GF for prediction for SVC for now.
  int layer_flags = AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
                    AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD |
                    AOM_EFLAG_NO_REF_ARF2;
  if (ksvc) {
    // Same pattern as case 8.
    layering_mode = 8;
    if (!is_key_frame)
      // No inter-layer prediction on inter-frames.
      layer_flags |= AOM_EFLAG_NO_REF_GF;
  }

  // Current patterns:
  if (number_spatial_layers == 1 && number_temporal_layers == 1)
    layering_mode = 0;
  if (number_spatial_layers == 1 && number_temporal_layers == 2)
    layering_mode = 1;
  if (number_spatial_layers == 1 && number_temporal_layers == 3)
    layering_mode = 2;
  else if (number_spatial_layers == 2 && number_temporal_layers == 1)
    layering_mode = 3;
  else if (number_spatial_layers == 3 && number_temporal_layers == 1)
    layering_mode = 4;
  else if (number_spatial_layers == 3 && number_temporal_layers == 3)
    layering_mode = 5;

  switch (layering_mode) {
    case 0:
      // 1-layer: update LAST on every frame, reference LAST and GF.
      layer_id->temporal_layer_id = 0;
      ref_frame_config->refresh[0] = 1;
      break;
    case 1:
      // 2-temporal layer.
      //    1    3    5
      //  0    2    4
      if (superframe_cnt % 2 == 0) {
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, reference LAST and GF.
        ref_frame_config->refresh[0] = 1;
      } else {
        layer_id->temporal_layer_id = 1;
        // No updates on layer 1, only reference LAST (TL0).
        layer_flags |= AOM_EFLAG_NO_REF_GF;
      }
      break;
    case 2:
      // 3-temporal layer:
      //   1    3   5    7
      //     2        6
      // 0        4        8
      if (superframe_cnt % 4 == 0) {
        // Base layer.
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, reference LAST and GF.
        ref_frame_config->refresh[0] = 1;
      } else if ((superframe_cnt - 1) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // First top layer: no updates, only reference LAST (TL0).
        layer_flags |= AOM_EFLAG_NO_REF_GF;
      } else if ((superframe_cnt - 2) % 4 == 0) {
        layer_id->temporal_layer_id = 1;
        // Middle layer (TL1): update LAST2, only reference LAST (TL0).
        ref_frame_config->refresh[1] = 1;
        layer_flags |= AOM_EFLAG_NO_REF_GF;
      } else if ((superframe_cnt - 3) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // Second top layer: no updates, only reference LAST.
        // Set buffer idx for LAST to slot 1, since that was the slot
        // updated in previous frame. So LAST is TL1 frame.
        ref_frame_config->ref_idx[0] = 1;
        ref_frame_config->ref_idx[1] = 0;
        layer_flags |= AOM_EFLAG_NO_REF_GF;
      }
      break;
    case 3:
      // 2 spatial layers, 1 temporal.
      layer_id->temporal_layer_id = 0;
      if (layer_id->spatial_layer_id == 0) {
        // Reference LAST, update LAST.
        ref_frame_config->refresh[0] = 1;
        layer_flags |= AOM_EFLAG_NO_REF_GF;
      } else if (layer_id->spatial_layer_id == 1) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1
        // and GOLDEN to slot 0. Update slot 1 (LAST).
        ref_frame_config->ref_idx[0] = 1;
        ref_frame_config->ref_idx[3] = 0;
        ref_frame_config->refresh[1] = 1;
      }
      break;
    case 4:
      // 3 spatial layers, 1 temporal.
      // Note for this case, we set the buffer idx for all references to be
      // either LAST or GOLDEN, which are always valid references, since decoder
      // will check if any of the 7 references is valid scale in
      // valid_ref_frame_size().
      layer_id->temporal_layer_id = 0;
      if (layer_id->spatial_layer_id == 0) {
        // Reference LAST, update LAST. Set all buffer_idx to 0.
        for (int i = 0; i < kInterRefsPerFrame; i++)
          ref_frame_config->ref_idx[i] = 0;
        ref_frame_config->refresh[0] = 1;
        layer_flags |= AOM_EFLAG_NO_REF_GF;
      } else if (layer_id->spatial_layer_id == 1) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1
        // and GOLDEN (and all other refs) to slot 0.
        // Update slot 1 (LAST).
        for (int i = 0; i < kInterRefsPerFrame; i++)
          ref_frame_config->ref_idx[i] = 0;
        ref_frame_config->ref_idx[0] = 1;
        ref_frame_config->refresh[1] = 1;
      } else if (layer_id->spatial_layer_id == 2) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2
        // and GOLDEN (and all other refs) to slot 1.
        // Update slot 2 (LAST).
        for (int i = 0; i < kInterRefsPerFrame; i++)
          ref_frame_config->ref_idx[i] = 1;
        ref_frame_config->ref_idx[0] = 2;
        ref_frame_config->refresh[2] = 1;
      }
      break;
    case 5:
      // 3 spatial and 3 temporal layer.
      // No overlap in buffer updates between TL2 and TL1.
      // TL2 updates slot 3 and 4, TL1 updates 5, 6, 7.
      if (superframe_cnt % 4 == 0) {
        // Base temporal layer.
        layer_id->temporal_layer_id = 0;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST, update LAST.
          // Set all buffer_idx to 0.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->refresh[0] = 1;
          layer_flags |= AOM_EFLAG_NO_REF_GF;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 0.
          // Update slot 1 (LAST).
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[0] = 1;
          ref_frame_config->refresh[1] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 1.
          // Update slot 2 (LAST).
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 1;
          ref_frame_config->ref_idx[0] = 2;
          ref_frame_config->refresh[2] = 1;
        }
      } else if ((superframe_cnt - 1) % 4 == 0) {
        // First top temporal enhancement layer.
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST (slot 0).
          // Set GOLDEN to slot 3 and update slot 3.
          // Set all other buffer_idx to slot 0.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[3] = 3;
          ref_frame_config->refresh[3] = 1;
          layer_flags |= AOM_EFLAG_NO_REF_GF;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 3.
          // Set LAST2 to slot 4 and Update slot 4.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 3;
          ref_frame_config->ref_idx[0] = 1;
          ref_frame_config->ref_idx[1] = 4;
          ref_frame_config->refresh[4] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 4.
          // No update.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 4;
          ref_frame_config->ref_idx[0] = 2;
        }
      } else if ((superframe_cnt - 2) % 4 == 0) {
        // Middle temporal enhancement layer.
        layer_id->temporal_layer_id = 1;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST.
          // Set all buffer_idx to 0.
          // Set GOLDEN to slot 5 and update slot 5.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[3] = 5;
          ref_frame_config->refresh[5] = 1;
          layer_flags |= AOM_EFLAG_NO_REF_GF;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 5.
          // Set LAST2 to slot 6 and update slot 6.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 5;
          ref_frame_config->ref_idx[0] = 1;
          ref_frame_config->ref_idx[2] = 6;
          ref_frame_config->refresh[6] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 6.
          // Set LAST2 to slot 6 and update slot 7.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 6;
          ref_frame_config->ref_idx[0] = 2;
          ref_frame_config->ref_idx[2] = 7;
          ref_frame_config->refresh[7] = 1;
        }
      } else if ((superframe_cnt - 3) % 4 == 0) {
        // Second top temporal enhancement layer.
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {
          // Set LAST to slot 5 and reference LAST.
          // Set GOLDEN to slot 3 and update slot 3.
          // Set all other buffer_idx to 0.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[0] = 5;
          ref_frame_config->ref_idx[3] = 3;
          ref_frame_config->refresh[3] = 1;
          layer_flags |= AOM_EFLAG_NO_REF_GF;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 6,
          // GOLDEN to slot 3. Set LAST2 to slot 4 and update slot 4.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[0] = 6;
          ref_frame_config->ref_idx[3] = 3;
          ref_frame_config->ref_idx[1] = 4;
          ref_frame_config->refresh[4] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 7,
          // GOLDEN to slot 4. No update.
          for (int i = 0; i < kInterRefsPerFrame; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[0] = 7;
          ref_frame_config->ref_idx[3] = 4;
        }
      }
      break;
  }
  return layer_flags;
}

int32_t LibaomAv1Encoder::Encode(
    const VideoFrame& frame,
    const std::vector<VideoFrameType>* frame_types) {
  if (!inited_ || encoded_image_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  for (int spatial_id = 0; spatial_id < num_spatial_layers_; spatial_id++) {
    aom_svc_layer_id_t layer_id;
    aom_svc_ref_frame_config_t ref_frame_config;
    memset(&layer_id, 0, sizeof(aom_svc_layer_id_t));
    memset(&ref_frame_config, 0, sizeof(aom_svc_ref_frame_config_t));

    keyframe_required_ =
        frame_types != nullptr &&
        absl::c_linear_search(*frame_types, VideoFrameType::kVideoFrameKey);

    // Convert input frame to I420, if needed.
    VideoFrame prepped_input_frame = frame;
    if (prepped_input_frame.video_frame_buffer()->type() !=
        VideoFrameBuffer::Type::kI420) {
      rtc::scoped_refptr<I420BufferInterface> converted_buffer(
          prepped_input_frame.video_frame_buffer()->ToI420());
      prepped_input_frame =
          VideoFrame(converted_buffer, frame.timestamp(),
                     frame.render_time_ms(), frame.rotation());
    }

    // Set frame_for_encode_ data pointers and strides.
    auto i420_buffer = prepped_input_frame.video_frame_buffer()->GetI420();
    frame_for_encode_->planes[AOM_PLANE_Y] =
        const_cast<unsigned char*>(i420_buffer->DataY());
    frame_for_encode_->planes[AOM_PLANE_U] =
        const_cast<unsigned char*>(i420_buffer->DataU());
    frame_for_encode_->planes[AOM_PLANE_V] =
        const_cast<unsigned char*>(i420_buffer->DataV());
    frame_for_encode_->stride[AOM_PLANE_Y] = i420_buffer->StrideY();
    frame_for_encode_->stride[AOM_PLANE_U] = i420_buffer->StrideU();
    frame_for_encode_->stride[AOM_PLANE_V] = i420_buffer->StrideV();

    const uint32_t duration =
        kRtpTicksPerSecond / static_cast<float>(encoder_settings_.maxFramerate);
    aom_enc_frame_flags_t flags = (keyframe_required_) ? AOM_EFLAG_FORCE_KF : 0;

    // Encode a frame (or spatial layer for SVC)
    // For SVC: we need the spatial_layer_id and superframe_count.

    layer_id.spatial_layer_id = spatial_id;
    // Set the reference flags, update flag and reference_map buffer index.
    // Also sets the temporal_layer_id.
    int ksvc = 0;  // Set this to 1 for K-SVC.
    flags = set_layer_pattern(num_spatial_layers_, num_temporal_layers_,
                              superframe_cnt_, &layer_id, &ref_frame_config,
                              spatial_id, keyframe_required_, ksvc);

    aom_codec_control(&ctx_, AV1E_SET_SVC_LAYER_ID, &layer_id);

    aom_codec_control(&ctx_, AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_frame_config);

    aom_codec_err_t ret = aom_codec_encode(&ctx_, frame_for_encode_,
                                           frame.timestamp(), duration, flags);
    if (ret != AOM_CODEC_OK) {
      RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encode returned " << ret
                          << " on aom_codec_encode.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (spatial_id == num_spatial_layers_ - 1)
      superframe_cnt_++;

    // Get encoded image data.
    EncodedImage encoded_image;
    encoded_image._completeFrame = true;
    aom_codec_iter_t iter = nullptr;
    int data_pkt_count = 0;
    while (const aom_codec_cx_pkt_t* pkt =
               aom_codec_get_cx_data(&ctx_, &iter)) {
      if (pkt->kind == AOM_CODEC_CX_FRAME_PKT && pkt->data.frame.sz > 0) {
        if (data_pkt_count > 0) {
          RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encoder returned more than "
                                 "one data packet for an input video frame.";
          Release();
        }
        // TODO(bugs.webrtc.org/11174): Remove this hack when
        // webrtc_pc_e2e::SingleProcessEncodedImageDataInjector not used or
        // fixed not to assume that encoded image transfered as is.
        const uint8_t* data = static_cast<const uint8_t*>(pkt->data.frame.buf);
        size_t size = pkt->data.frame.sz;
        if (size > 2 && data[0] == 0b0'0010'010 && data[1] == 0) {
          // Typically frame starts with a Temporal Delimter OBU of size 0 that
          // is not need by any component in webrtc and discarded during rtp
          // packetization. Before discarded it confuses test framework that
          // assumes received encoded frame is exactly same as sent frame.
          data += 2;
          size -= 2;
        }
        encoded_image.SetEncodedData(EncodedImageBuffer::Create(data, size));

        bool is_key_frame = ((pkt->data.frame.flags & AOM_EFLAG_FORCE_KF) != 0);
        encoded_image._frameType = is_key_frame
                                       ? VideoFrameType::kVideoFrameKey
                                       : VideoFrameType::kVideoFrameDelta;
        encoded_image.SetTimestamp(frame.timestamp());
        encoded_image.capture_time_ms_ = frame.render_time_ms();
        encoded_image.rotation_ = frame.rotation();
        encoded_image.content_type_ = VideoContentType::UNSPECIFIED;
        // If encoded image width/height info are added to aom_codec_cx_pkt_t,
        // use those values in lieu of the values in frame.
        encoded_image._encodedHeight = frame.height();
        encoded_image._encodedWidth = frame.width();
        encoded_image.timing_.flags = VideoSendTiming::kInvalid;
        int qp = -1;
        ret = aom_codec_control(&ctx_, AOME_GET_LAST_QUANTIZER, &qp);
        if (ret != AOM_CODEC_OK) {
          RTC_LOG(LS_WARNING) << "LibaomAv1Encoder::Encode returned " << ret
                              << " on control AOME_GET_LAST_QUANTIZER.";
          return WEBRTC_VIDEO_CODEC_ERROR;
        }
        encoded_image.qp_ = qp;
        encoded_image.SetColorSpace(frame.color_space());
        ++data_pkt_count;
      }
    }

    // Deliver encoded image data.
    if (encoded_image.size() > 0) {
      CodecSpecificInfo codec_specific_info;
      encoded_image_callback_->OnEncodedImage(encoded_image,
                                              &codec_specific_info, nullptr);
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void LibaomAv1Encoder::SetRates(const RateControlParameters& parameters) {
  if (!inited_) {
    RTC_LOG(LS_WARNING) << "SetRates() while encoder is not initialized";
    return;
  }
  if (parameters.framerate_fps < kMinimumFrameRate) {
    RTC_LOG(LS_WARNING) << "Unsupported framerate (must be >= "
                        << kMinimumFrameRate
                        << " ): " << parameters.framerate_fps;
    return;
  }
  if (parameters.bitrate.get_sum_bps() == 0) {
    RTC_LOG(LS_WARNING) << "Attempt to set target bit rate to zero";
    return;
  }

  // Check input target bit rate value.
  uint32_t rc_target_bitrate_kbps = parameters.bitrate.get_sum_kbps();
  if (encoder_settings_.maxBitrate > 0)
    RTC_DCHECK_LE(rc_target_bitrate_kbps, encoder_settings_.maxBitrate);
  RTC_DCHECK_GE(rc_target_bitrate_kbps, encoder_settings_.minBitrate);

  // Set target bit rate.
  cfg_.rc_target_bitrate = rc_target_bitrate_kbps;

  for (size_t sl_idx = 0; sl_idx < num_spatial_layers_; ++sl_idx) {
    for (size_t tl_idx = 0; tl_idx < num_temporal_layers_; ++tl_idx) {
      svc_params_.layer_target_bitrate[sl_idx * num_temporal_layers_ + tl_idx] =
          parameters.bitrate.GetTemporalLayerSum(sl_idx, tl_idx) / 1000;
    }
  }

  aom_codec_control(&ctx_, AV1E_SET_SVC_PARAMS, &svc_params_);

  // Set frame rate to closest integer value.
  encoder_settings_.maxFramerate =
      static_cast<uint32_t>(parameters.framerate_fps + 0.5);

  // Update encoder context.
  aom_codec_err_t error_code = aom_codec_enc_config_set(&ctx_, &cfg_);
  if (error_code != AOM_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "Error configuring encoder, error code: "
                        << error_code;
  }
}

VideoEncoder::EncoderInfo LibaomAv1Encoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.supports_native_handle = false;
  info.implementation_name = "libaom";
  info.has_trusted_rate_controller = true;
  info.is_hardware_accelerated = false;
  info.scaling_settings = VideoEncoder::ScalingSettings(kMinQindex, kMaxQindex);
  return info;
}

}  // namespace

const bool kIsLibaomAv1EncoderSupported = true;

std::unique_ptr<VideoEncoder> CreateLibaomAv1Encoder() {
  return std::make_unique<LibaomAv1Encoder>();
}

}  // namespace webrtc
