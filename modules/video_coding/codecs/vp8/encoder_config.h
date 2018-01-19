/* Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_VP8_ENCODER_CONFIG_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_ENCODER_CONFIG_H_

#include "modules/video_coding/codecs/vp8/temporal_layers.h"

#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

namespace webrtc {

bool UpdateVpxConfiguration(TemporalLayers* temporal_layers,
                            vpx_codec_enc_cfg_t* cfg);

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_CODECS_VP8_ENCODER_CONFIG_H_
