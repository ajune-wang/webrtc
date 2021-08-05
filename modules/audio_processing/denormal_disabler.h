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

namespace webrtc {

// Enables the hardware way to flush denormals to zero as they can very
// seriously impact performance on x86. At destruction time restores the
// denormals handling state read by the ctor.
class DenormalDisabler {
 public:
  // Ctor. If `enabled` is false, this class does nothing.
  explicit DenormalDisabler(bool enabled)
      : enabled_(enabled), status_word_(enabled_ ? ReadStatusWord() : 0) {
#if defined(WEBRTC_ARCH_X86_FAMILY) || defined(WEBRTC_ARCH_ARM_FAMILY)
    if (enabled_) {
      SetStatusWord(status_word_ | kFtzControlBits);
    }
#endif
  }
  DenormalDisabler(const DenormalDisabler&) = delete;
  DenormalDisabler& operator=(const DenormalDisabler&) = delete;
  ~DenormalDisabler() {
    if (enabled_) {
      SetStatusWord(status_word_);
    }
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(DenormalDisabler, DoNothingIfDisabled);

  const bool enabled_;
  const int status_word_;

#if defined(WEBRTC_ARCH_X86_FAMILY)
  // Flush-to-zero and denormals-are-zero control bits.
  static constexpr int kFtzControlBits = 0x8040;
#elif defined(WEBRTC_ARCH_ARM_FAMILY)
  // Flush-to-zero control bit.
  static constexpr int kFtzControlBits = 1 << 24;
#endif

  // Only for testing. Enables denormals on the CPU.
  static inline void EnableDenormals() {
#if defined(WEBRTC_ARCH_X86_FAMILY) || defined(WEBRTC_ARCH_ARM_FAMILY)
    SetStatusWord(ReadStatusWord() & ~kFtzControlBits);
#endif
  }

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
