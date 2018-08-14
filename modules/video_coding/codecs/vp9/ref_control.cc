/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/ref_control.h"

#include <vector>

namespace webrtc {

namespace {
const size_t kSpatialRefBufIdx = 7;
}  // namespace

vpx_svc_ref_frame_config_t ReferenceControl::SetFrameReferences(
    const std::vector<ReferenceConfig> layers) {
  vpx_svc_ref_frame_config_t enc_layer_conf = {{0}};

  for (size_t i = 0; i < layers.size(); ++i) {
    const ReferenceConfig& layer = layers[i];
    const size_t gof_idx = layer.frames_since_key % layer.gof_size;
    const size_t sl_idx = layer.spatial_idx;

    if (layer.frames_since_key > 0) {
      enc_layer_conf.lst_fb_idx[sl_idx] = layer.ref_buf_idx[gof_idx];
    } else if (i > 0) {
      RTC_DCHECK(layer.inter_layer_pred);
      enc_layer_conf.lst_fb_idx[sl_idx] =
          ffs(enc_layer_conf.update_buffer_slot[layers[i - 1].spatial_idx]) - 1;
    }

    if (layer.inter_layer_pred) {
      RTC_DCHECK_NE(
          enc_layer_conf.update_buffer_slot[layers[i - 1].spatial_idx], 0);
      enc_layer_conf.gld_fb_idx[sl_idx] =
          ffs(enc_layer_conf.update_buffer_slot[layers[i - 1].spatial_idx]) - 1;
    } else {
      enc_layer_conf.gld_fb_idx[sl_idx] = enc_layer_conf.lst_fb_idx[sl_idx];
    }

    enc_layer_conf.alt_fb_idx[sl_idx] = enc_layer_conf.lst_fb_idx[sl_idx];

    if (layer.ref_frame_flag[gof_idx]) {
      enc_layer_conf.update_buffer_slot[sl_idx] = 1
                                                  << layer.upd_buf_idx[gof_idx];
    } else if (i + 1 < layers.size() && layers[i + 1].inter_layer_pred) {
      enc_layer_conf.update_buffer_slot[sl_idx] = 1 << kSpatialRefBufIdx;
    }
  }

  return enc_layer_conf;
}

}  // namespace webrtc
