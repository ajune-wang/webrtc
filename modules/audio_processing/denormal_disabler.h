/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_DENORMAL_DISABLER_H_
#define MODULES_AUDIO_PROCESSING_DENORMAL_DISABLER_H_

#include "rtc_base/gtest_prod_util.h"
#include "rtc_base/system/arch.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

// Enables the hardware way to flush denormals to zero as they can very
// seriously impact performance on x86. At destruction time restores the
// denormals handling state read by the ctor.
class DenormalDisabler {
 public:
  DenormalDisabler()
      : enabled_(
            !field_trial::IsEnabled("WebRTC-ApmDenormalDisablerKillSwitch")),
        status_word_(enabled_ ? ReadStatusWord() : 0) {
    if (enabled_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
      // Set flush-to-zero and denormals-are-zero control bits.
      constexpr int kFtzDaz = 0x8040;
      SetStatusWord(status_word_ | kFtzDaz);
#elif defined(WEBRTC_ARCH_ARM_FAMILY)
      // Set flush-to-zero control bit.
      constexpr int kFtz = 1 << 24;
      SetStatusWord(status_word_ | kFtz);
#endif
    }
  }
  DenormalDisabler(const DenormalDisabler&) = delete;
  DenormalDisabler& operator=(const DenormalDisabler&) = delete;
  ~DenormalDisabler() {
    if (enabled_)
      SetStatusWord(status_word_);
  }

  bool enabled() const { return enabled_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(DenormalDisabler, ZeroDenormals);
  FRIEND_TEST_ALL_PREFIXES(DenormalDisabler, InfNotZeroed);
  FRIEND_TEST_ALL_PREFIXES(DenormalDisabler, NanNotZeroed);

  const bool enabled_;
  const int status_word_;

  static inline int ReadStatusWord() {
    int result;
#if defined(WEBRTC_ARCH_X86_FAMILY)
    asm volatile("stmxcsr %0" : "=m"(result));
#elif defined(WEBRTC_ARCH_ARM_FAMILY) && defined(WEBRTC_ARCH_32_BITS)
    asm volatile("vmrs %[result], FPSCR" : [result] "=r"(result));
#elif defined(WEBRTC_ARCH_ARM_FAMILY) && defined(WEBRTC_ARCH_64_BITS)
    asm volatile("mrs %x[result], FPCR" : [result] "=r"(result));
#else
    // Platform not supported.
    result = 0;
#endif
    return result;
  }

  static inline void SetStatusWord(int status_word) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    const int tmp = status_word;
    asm volatile("ldmxcsr %0" : : "m"(tmp));
#elif defined(WEBRTC_ARCH_ARM_FAMILY) && defined(WEBRTC_ARCH_32_BITS)
    asm volatile("vmsr FPSCR, %[src]" : : [src] "r"(status_word));
#elif defined(WEBRTC_ARCH_ARM_FAMILY) && defined(WEBRTC_ARCH_64_BITS)
    asm volatile("msr FPCR, %x[src]" : : [src] "r"(status_word));
#endif
  }
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_DENORMAL_DISABLER_H_
