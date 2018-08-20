/* Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_REF_CONTROL_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_REF_CONTROL_H_

#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "vpx/vp8cx.h"

namespace webrtc {
class ReferenceControl {
 public:
  struct ReferenceConfig {
    ReferenceConfig(size_t gof_size,
                    size_t spatial_idx,
                    bool inter_layer_pred,
                    std::vector<uint8_t> temporal_idx,
                    std::vector<bool> ref_frame_flag,
                    std::vector<uint8_t> ref_buf_idx,
                    std::vector<uint8_t> upd_buf_idx)
        : gof_size(gof_size),
          spatial_idx(spatial_idx),
          inter_layer_pred(inter_layer_pred),
          temporal_idx(temporal_idx),
          ref_frame_flag(ref_frame_flag),
          ref_buf_idx(ref_buf_idx),
          upd_buf_idx(upd_buf_idx),
          frames_since_key(0) {}
    size_t gof_size;
    size_t spatial_idx;
    bool inter_layer_pred;
    std::vector<uint8_t> temporal_idx;
    std::vector<bool> ref_frame_flag;
    std::vector<uint8_t> ref_buf_idx;
    std::vector<uint8_t> upd_buf_idx;
    size_t frames_since_key;
  };

  static vpx_svc_ref_frame_config_t SetFrameReferences(
      const std::vector<ReferenceConfig> layers);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP9_REF_CONTROL_H_
