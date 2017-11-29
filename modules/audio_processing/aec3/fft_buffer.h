/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_FFT_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_FFT_BUFFER_H_

#include <vector>

#include "modules/audio_processing/aec3/fft_data.h"
#include "rtc_base/checks.h"

namespace webrtc {

// Struct for bundling a circular buffer of FftData objects together with the
// read and write indices.
struct FftBuffer {
  explicit FftBuffer(size_t size);
  ~FftBuffer();
  void Clear();

  size_t IncIndex(size_t index) {
    return index < (buffer.size() - 1) ? index + 1 : 0;
  }

  size_t DecIndex(size_t index) {
    return index > 0 ? index - 1 : buffer.size() - 1;
  }

  size_t OffsetIndex(size_t index, int offset) {
    RTC_DCHECK_GE(buffer.size(), offset);
    return (buffer.size() + index + offset) % buffer.size();
  }

  void UpdateLastInsertIndex(int offset) {
    last_insert = OffsetIndex(last_insert, offset);
  }
  void IncLastInsertIndex() { last_insert = IncIndex(last_insert); }
  void DecLastInsertIndex() { last_insert = DecIndex(last_insert); }
  void UpdateNextReadIndex(int offset) {
    next_read = OffsetIndex(next_read, offset);
  }
  void IncNextReadIndex() { next_read = IncIndex(next_read); }
  void DecNextReadIndex() { next_read = DecIndex(next_read); }

  std::vector<FftData> buffer;
  size_t last_insert = 0;
  size_t next_read = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_FFT_BUFFER_H_
