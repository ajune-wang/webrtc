/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H265_H265_FRAME_BUILDER_H_
#define COMMON_VIDEO_H265_H265_FRAME_BUILDER_H_

#include <stddef.h>
#include <stdint.h>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class H265AnnexBBitstreamBuilder;

// Builds a H.265 key frame with a single NALU containing a VPS, SPS and PPS.
// Most of the params are fixed while the width, height, number of temporal
// layers and QP can be set. The log2_max_pic_order_cnt_lsb_minus4 is set to 0.
RTC_EXPORT void BuildKeyFrame(H265AnnexBBitstreamBuilder& builder,
                              size_t width,
                              size_t height,
                              uint8_t num_temporal_layers,
                              int qp,
                              size_t frame_size_bytes);

// Builds a H.265 delta frame with a single NALU containing a slice.
// Note: It is expected a preceeding BuildKeyFrame call is made to set up the
// dependent parameter sets(VPS/SPS/PPS) used by this frame. Since BuildKeyFrame
// sets the log2_max_pic_order_cnt_lsb_minus4 to 0, it is expected that each
// time BuildDeltaFrame() is called, the wrapped_on_16_poc_lsb is increased by 1
// and wraps back to 0 if it reaches 16. The first BuildDeltaFrame() call should
// have |wrapped_on_16_poc_lsb| set to 1.
RTC_EXPORT void BuildDeltaFrame(H265AnnexBBitstreamBuilder& builder,
                                uint8_t temporal_layer_id,
                                int qp,
                                size_t frame_size_bytes,
                                uint8_t wrapped_on_16_poc_lsb);

}  // namespace webrtc

#endif  // COMMON_VIDEO_H265_H265_FRAME_BUILDER_H_
