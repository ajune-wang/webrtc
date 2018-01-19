/* Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/encoder_config.h"

namespace webrtc {

static_assert(
    VP8_TS_MAX_PERIODICITY == VPX_TS_MAX_PERIODICITY,
    "VP8_TS_MAX_PERIODICITY must be kept in sync with the constant in libvpx.");
static_assert(
    VP8_TS_MAX_LAYERS == VPX_TS_MAX_LAYERS,
    "VP8_TS_MAX_LAYERS must be kept in sync with the constant in libvpx.");

static Vp8EncoderConfig GetEncoderConfig(vpx_codec_enc_cfg* vpx_config) {
  Vp8EncoderConfig config;

  config.ts_number_layers = vpx_config->ts_number_layers;
  memcpy(config.ts_target_bitrate, vpx_config->ts_target_bitrate,
         sizeof(unsigned int) * VP8_TS_MAX_LAYERS);
  memcpy(config.ts_rate_decimator, vpx_config->ts_rate_decimator,
         sizeof(unsigned int) * VP8_TS_MAX_LAYERS);
  config.ts_periodicity = vpx_config->ts_periodicity;
  memcpy(config.ts_layer_id, vpx_config->ts_layer_id,
         sizeof(unsigned int) * VP8_TS_MAX_PERIODICITY);
  config.rc_target_bitrate = vpx_config->rc_target_bitrate;
  config.rc_min_quantizer = vpx_config->rc_min_quantizer;
  config.rc_max_quantizer = vpx_config->rc_max_quantizer;

  return config;
}

static void FillInEncoderConfig(vpx_codec_enc_cfg* vpx_config,
                                const Vp8EncoderConfig& config) {
  vpx_config->ts_number_layers = config.ts_number_layers;
  memcpy(vpx_config->ts_target_bitrate, config.ts_target_bitrate,
         sizeof(unsigned int) * VP8_TS_MAX_LAYERS);
  memcpy(vpx_config->ts_rate_decimator, config.ts_rate_decimator,
         sizeof(unsigned int) * VP8_TS_MAX_LAYERS);
  vpx_config->ts_periodicity = config.ts_periodicity;
  memcpy(vpx_config->ts_layer_id, config.ts_layer_id,
         sizeof(unsigned int) * VP8_TS_MAX_PERIODICITY);
  vpx_config->rc_target_bitrate = config.rc_target_bitrate;
  vpx_config->rc_min_quantizer = config.rc_min_quantizer;
  vpx_config->rc_max_quantizer = config.rc_max_quantizer;
}

bool UpdateVpxConfiguration(TemporalLayers* temporal_layers,
                            vpx_codec_enc_cfg_t* cfg) {
  Vp8EncoderConfig config = GetEncoderConfig(cfg);
  const bool res = temporal_layers->UpdateConfiguration(&config);
  if (res)
    FillInEncoderConfig(cfg, config);
  return res;
}

}  // namespace webrtc
