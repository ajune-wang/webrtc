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
    last_insert_index = OffsetIndex(last_insert_index, offset);
  }
  void IncLastInsertIndex() { last_insert_index = IncIndex(last_insert_index); }
  void DecLastInsertIndex() { last_insert_index = DecIndex(last_insert_index); }
  void UpdateNextReadIndex(int offset) {
    next_read_index = OffsetIndex(next_read_index, offset);
  }
  void IncNextReadIndex() { next_read_index = IncIndex(next_read_index); }
  void DecNextReadIndex() { next_read_index = DecIndex(next_read_index); }

  std::vector<FftData> buffer;
  size_t last_insert_index = 0;
  size_t next_read_index = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_FFT_BUFFER_H_
