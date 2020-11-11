/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/common.h"

#include "rtc_base/system/arch.h"
#include "system_wrappers/include/cpu_features_wrapper.h"

namespace webrtc {
namespace rnn_vad {

bool IsOptimizationAvailable(Optimization optimization) {
  switch (optimization) {
    case Optimization::kAvx2:
#if defined(WEBRTC_ARCH_X86_FAMILY)
      return GetCPUInfo(kAVX2) != 0;
#else
      return false;
#endif
    case Optimization::kSse2:
#if defined(WEBRTC_ARCH_X86_FAMILY)
      return GetCPUInfo(kSSE2) != 0;
#else
      return false;
#endif
    case Optimization::kNeon:
#if defined(WEBRTC_HAS_NEON)
      return true;
#else
      return false;
#endif
    case Optimization::kNone:
      return true;
  }
}

Optimization GetBestOptimization(uint32_t has_implementation_mask,
                                 uint32_t disabled_mask) {
  // An optimization can be used if not disabled and if its implementation is
  // available.
  const auto can_use = [has_implementation_mask,
                        disabled_mask](Optimization o) -> bool {
    return (has_implementation_mask & o) && !(disabled_mask & o);
  };
#if defined(WEBRTC_ARCH_X86_FAMILY)
  constexpr Optimization kPreferredOrder[]{Optimization::kAvx2,
                                           Optimization::kSse2};
  for (Optimization best : kPreferredOrder) {
    if (IsOptimizationAvailable(best) && can_use(best)) {
      return best;
    }
  }
#elif defined(WEBRTC_HAS_NEON)
  if (IsOptimizationAvailable(Optimization::kNeon) &&
      can_use(Optimization::kNeon)) {
    return Optimization::kNeon;
  }
#endif
  return Optimization::kNone;
}

}  // namespace rnn_vad
}  // namespace webrtc
