/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/cpu_features.h"

#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/arch.h"
#include "system_wrappers/include/cpu_features_wrapper.h"

namespace webrtc {

std::string AvailableCpuFeatures::ToString() const {
  char buf[64];
  rtc::SimpleStringBuilder builder(buf);
  builder << "{ sse2: " << (sse2 ? '1':'0') <<
             ", avx2: " << (avx2 ? '1':'0') <<
             ", neon: " << (neon ? '1':'0') << " }";
  return builder.str();
}

// Detects available CPU features.
AvailableCpuFeatures GetAvailableCpuFeatures() {
  AvailableCpuFeatures feature;
#if defined(WEBRTC_ARCH_X86_FAMILY)
  feature.sse2 = GetCPUInfo(kSSE2) != 0;
  feature.avx2 = GetCPUInfo(kAVX2) != 0;
#endif
#if defined(WEBRTC_HAS_NEON)
  feature.neon = true;
#endif
  return feature;
}

}  // namespace webrtc
