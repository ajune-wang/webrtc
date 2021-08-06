/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_INCLUDE_DENORMAL_DISABLER_H_
#define SYSTEM_WRAPPERS_INCLUDE_DENORMAL_DISABLER_H_

#include "rtc_base/gtest_prod_util.h"
#include "rtc_base/system/arch.h"

namespace webrtc {

#if defined(WEBRTC_ARCH_X86_FAMILY) && defined(__clang__)
#define WEBRTC_DENORMAL_DISABLER_X86_SUPPORTED
#endif

#if defined(WEBRTC_DENORMAL_DISABLER_X86_SUPPORTED) || \
    defined(WEBRTC_ARCH_ARM_FAMILY)
#define WEBRTC_DENORMAL_DISABLER_SUPPORTED
#endif

// Enables the hardware (HW) way to flush denormals to zero as they can very
// seriously impact performance. At destruction time restores the denormals
// handling state read by the ctor; hence, supports nested calls.
// Equals a no-op if the architecture is not x86 or ARM or if the compiler is
// not CLANG.
//
// Example usage:
//
// void Foo() {
//   DenormalDisabler d;
//   ...
// }
class DenormalDisabler {
 public:
  // Ctor. If `enabled` is true and architecture and compiler are supported,
  // stores the HW settings for denormals, disables denormals and sets
  // `enabled_` to true. Otherwise, only sets `enabled_` to false.
  explicit DenormalDisabler(bool enabled);
  DenormalDisabler(const DenormalDisabler&) = delete;
  DenormalDisabler& operator=(const DenormalDisabler&) = delete;
  // Dtor. If `enabled_` is true, restores the denormals HW settings read by the
  // ctor before denormals were disabled. Otherwise it's a no-op.
  ~DenormalDisabler();

  // Returns true if the ctor disabled denormals.
  bool enabled() const { return enabled_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(DenormalDisabler, DoNotZeroDenormalsIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(DenormalDisabler, RestoreDenormalsEnabled);

  const bool enabled_;
  const int status_word_;

  // Only for testing. Enables denormals on the CPU if the architecture and the
  // compiler are supported.
  static void EnableDenormals();
};

}  // namespace webrtc

#endif  // SYSTEM_WRAPPERS_INCLUDE_DENORMAL_DISABLER_H_
