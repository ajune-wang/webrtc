/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the implementation of functions
 * WebRtcSpl_MaxAbsValueW16()
 * WebRtcSpl_MaxAbsValueW32()
 * WebRtcSpl_MaxValueW16C()
 * WebRtcSpl_MaxValueW32C()
 * WebRtcSpl_MinValueW16C()
 * WebRtcSpl_MinValueW32C()
 * WebRtcSpl_MaxIndexW16()
 * WebRtcSpl_MaxIndexW32()
 * WebRtcSpl_MinIndexW16()
 * WebRtcSpl_MinIndexW32()
 * WebRtcSpl_MinMaxW16()
 * WebRtcSpl_MinMaxW32()
 *
 */

#include <stdlib.h>

#include "rtc_base/checks.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"

// TODO(kma): Move the next six functions into min_max_operations_c.c.

// Maximum absolute value of word16 vector.
int16_t WebRtcSpl_MaxAbsValueW16(const int16_t* data, size_t length) {
#if defined(MIPS32_LE)
  return WebRtcSpl_MaxAbsValueW16_mips(vector, length);
#else
  int16_t min_val, max_val;
  WebRtcSpl_MinMaxW16(data, length, &min_val, &max_val);
  if (min_val == WEBRTC_SPL_WORD16_MIN) {
    return WEBRTC_SPL_WORD16_MAX;
  }
  if (min_val < -max_val) {
    return -min_val;
  }
  return max_val;
#endif
}

// Maximum absolute value of word32 vector.
int32_t WebRtcSpl_MaxAbsValueW32(const int32_t* data, size_t length) {
#if defined(MIPS_DSP_R1_LE)
  return WebRtcSpl_MaxAbsValueW32_mips(vector, length);
#else
  int32_t min_val, max_val;
  WebRtcSpl_MinMaxW32(data, length, &min_val, &max_val);
  if (min_val == WEBRTC_SPL_WORD32_MIN) {
    return WEBRTC_SPL_WORD32_MAX;
  }
  if (min_val < -max_val) {
    return -min_val;
  }
  return max_val;
#endif
}

// Maximum value of word16 vector. C version for generic platforms.
int16_t WebRtcSpl_MaxValueW16C(const int16_t* vector, size_t length) {
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] > maximum)
      maximum = vector[i];
  }
  return maximum;
}

// Maximum value of word32 vector. C version for generic platforms.
int32_t WebRtcSpl_MaxValueW32C(const int32_t* vector, size_t length) {
  int32_t maximum = WEBRTC_SPL_WORD32_MIN;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] > maximum)
      maximum = vector[i];
  }
  return maximum;
}

// Minimum value of word16 vector. C version for generic platforms.
int16_t WebRtcSpl_MinValueW16C(const int16_t* vector, size_t length) {
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum)
      minimum = vector[i];
  }
  return minimum;
}

// Minimum value of word32 vector. C version for generic platforms.
int32_t WebRtcSpl_MinValueW32C(const int32_t* vector, size_t length) {
  int32_t minimum = WEBRTC_SPL_WORD32_MAX;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum)
      minimum = vector[i];
  }
  return minimum;
}

int16_t WebRtcSpl_MaxAbsElementW16(const int16_t* data, size_t length) {
  int16_t min_val, max_val;
  WebRtcSpl_MinMaxW16(data, length, &min_val, &max_val);
  if (min_val == max_val || min_val < -max_val) {
    return min_val;
  }
  return max_val;
}

// Index of maximum value in a word16 vector.
size_t WebRtcSpl_MaxIndexW16(const int16_t* vector, size_t length) {
  size_t i = 0, index = 0;
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] > maximum) {
      maximum = vector[i];
      index = i;
    }
  }

  return index;
}

// Index of maximum value in a word32 vector.
size_t WebRtcSpl_MaxIndexW32(const int32_t* vector, size_t length) {
  size_t i = 0, index = 0;
  int32_t maximum = WEBRTC_SPL_WORD32_MIN;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] > maximum) {
      maximum = vector[i];
      index = i;
    }
  }

  return index;
}

// Index of minimum value in a word16 vector.
size_t WebRtcSpl_MinIndexW16(const int16_t* vector, size_t length) {
  size_t i = 0, index = 0;
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum) {
      minimum = vector[i];
      index = i;
    }
  }

  return index;
}

// Index of minimum value in a word32 vector.
size_t WebRtcSpl_MinIndexW32(const int32_t* vector, size_t length) {
  size_t i = 0, index = 0;
  int32_t minimum = WEBRTC_SPL_WORD32_MAX;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum) {
      minimum = vector[i];
      index = i;
    }
  }

  return index;
}

// Finds both the minimum and maximum elements in an array of 16-bit integers.
void WebRtcSpl_MinMaxW16(const int16_t* data, size_t length,
                         int16_t* min_val, int16_t* max_val) {
#if defined(WEBRTC_HAS_NEON)
  return WebRtcSpl_MinMaxW16Neon(data, length, min_val, max_val);
#else
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (data[i] < minimum)
      minimum = data[i];
    if (data[i] > maximum)
      maximum = data[i];
  }
  *min_val = minimum;
  *max_val = maximum;
#endif
}

void WebRtcSpl_MinMaxW32(const int32_t* data, size_t length, int32_t* min_val,
                         int32_t* max_val) {
#if defined(WEBRTC_HAS_NEON)
  return WebRtcSpl_MinMaxW32Neon(data, length, min_val, max_val);
#else
  int32_t minimum = WEBRTC_SPL_WORD32_MAX;
  int32_t maximum = WEBRTC_SPL_WORD32_MIN;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (data[i] < minimum)
      minimum = data[i];
    if (data[i] > maximum)
      maximum = data[i];
  }
  *min_val = minimum;
  *max_val = maximum;
#endif
}
