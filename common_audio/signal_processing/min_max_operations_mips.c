/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/checks.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"


// Maximum value of word16 vector. Version for MIPS platform.
int16_t WebRtcSpl_MaxValueW16_mips(const int16_t* vector, size_t length) {
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;
  int tmp1;
  int16_t value;

  RTC_DCHECK_GT(length, 0);

  __asm__ volatile (
    ".set push                                                        \n\t"
    ".set noreorder                                                   \n\t"

   "1:                                                                \n\t"
    "lh         %[value],         0(%[vector])                        \n\t"
    "addiu      %[length],        %[length],          -1              \n\t"
    "slt        %[tmp1],          %[maximum],         %[value]        \n\t"
    "movn       %[maximum],       %[value],           %[tmp1]         \n\t"
    "bgtz       %[length],        1b                                  \n\t"
    " addiu     %[vector],        %[vector],          2               \n\t"
    ".set pop                                                         \n\t"

    : [tmp1] "=&r" (tmp1), [maximum] "+r" (maximum), [value] "=&r" (value)
    : [vector] "r" (vector), [length] "r" (length)
    : "memory"
  );

  return maximum;
}

// Maximum value of word32 vector. Version for MIPS platform.
int32_t WebRtcSpl_MaxValueW32_mips(const int32_t* vector, size_t length) {
  int32_t maximum = WEBRTC_SPL_WORD32_MIN;
  int tmp1, value;

  RTC_DCHECK_GT(length, 0);

  __asm__ volatile (
    ".set push                                                        \n\t"
    ".set noreorder                                                   \n\t"

   "1:                                                                \n\t"
    "lw         %[value],         0(%[vector])                        \n\t"
    "addiu      %[length],        %[length],          -1              \n\t"
    "slt        %[tmp1],          %[maximum],         %[value]        \n\t"
    "movn       %[maximum],       %[value],           %[tmp1]         \n\t"
    "bgtz       %[length],        1b                                  \n\t"
    " addiu     %[vector],        %[vector],          4               \n\t"

    ".set pop                                                         \n\t"

    : [tmp1] "=&r" (tmp1), [maximum] "+r" (maximum), [value] "=&r" (value)
    : [vector] "r" (vector), [length] "r" (length)
    : "memory"
  );

  return maximum;
}

// Minimum value of word16 vector. Version for MIPS platform.
int16_t WebRtcSpl_MinValueW16_mips(const int16_t* vector, size_t length) {
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;
  int tmp1;
  int16_t value;

  RTC_DCHECK_GT(length, 0);

  __asm__ volatile (
    ".set push                                                        \n\t"
    ".set noreorder                                                   \n\t"

   "1:                                                                \n\t"
    "lh         %[value],         0(%[vector])                        \n\t"
    "addiu      %[length],        %[length],          -1              \n\t"
    "slt        %[tmp1],          %[value],           %[minimum]      \n\t"
    "movn       %[minimum],       %[value],           %[tmp1]         \n\t"
    "bgtz       %[length],        1b                                  \n\t"
    " addiu     %[vector],        %[vector],          2               \n\t"

    ".set pop                                                         \n\t"

    : [tmp1] "=&r" (tmp1), [minimum] "+r" (minimum), [value] "=&r" (value)
    : [vector] "r" (vector), [length] "r" (length)
    : "memory"
  );

  return minimum;
}

// Minimum value of word32 vector. Version for MIPS platform.
int32_t WebRtcSpl_MinValueW32_mips(const int32_t* vector, size_t length) {
  int32_t minimum = WEBRTC_SPL_WORD32_MAX;
  int tmp1, value;

  RTC_DCHECK_GT(length, 0);

  __asm__ volatile (
    ".set push                                                        \n\t"
    ".set noreorder                                                   \n\t"

   "1:                                                                \n\t"
    "lw         %[value],         0(%[vector])                        \n\t"
    "addiu      %[length],        %[length],          -1              \n\t"
    "slt        %[tmp1],          %[value],           %[minimum]      \n\t"
    "movn       %[minimum],       %[value],           %[tmp1]         \n\t"
    "bgtz       %[length],        1b                                  \n\t"
    " addiu     %[vector],        %[vector],          4               \n\t"

    ".set pop                                                         \n\t"

    : [tmp1] "=&r" (tmp1), [minimum] "+r" (minimum), [value] "=&r" (value)
    : [vector] "r" (vector), [length] "r" (length)
    : "memory"
  );

  return minimum;
}
