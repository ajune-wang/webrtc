/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/aec3_common.h"

#include "typedefs.h"  // NOLINT(build/include)
#include "system_wrappers/include/cpu_features_wrapper.h"

namespace webrtc {

Aec3Optimization DetectOptimization() {
#if defined(WEBRTC_ARCH_X86_FAMILY)
  if (WebRtc_GetCPUInfo(kSSE2) != 0) {
    return Aec3Optimization::kSse2;
  }
#endif

#if defined(WEBRTC_HAS_NEON)
  return Aec3Optimization::kNeon;
#endif

  return Aec3Optimization::kNone;
}

size_t GetDownSampledBufferSize(size_t down_sampling_factor,
                                size_t matched_filter_size_sub_blocks,
                                size_t filter_overlap_sub_blocks,
                                size_t num_matched_filters) {
  size_t min_size =
      kBlockSize / down_sampling_factor *
      ((matched_filter_size_sub_blocks - filter_overlap_sub_blocks) *
           num_matched_filters +
       matched_filter_size_sub_blocks + 1);
  return pow(2, ceil(log(min_size) / log(2)));
}

size_t GetRenderDelayBufferSize(size_t down_sampling_factor,
                                size_t matched_filter_size_sub_blocks,
                                size_t filter_overlap_sub_blocks,
                                size_t num_matched_filters,
                                size_t filter_length_blocks) {
  return GetDownSampledBufferSize(
             down_sampling_factor, matched_filter_size_sub_blocks,
             filter_overlap_sub_blocks, num_matched_filters) /
             (kBlockSize / down_sampling_factor) +
         filter_length_blocks + 1;
}

}  // namespace webrtc
