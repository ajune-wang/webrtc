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

namespace webrtc {

struct FftBuffer {
  explicit FftBuffer(size_t size);
  ~FftBuffer();
  void Clear();

  std::vector<FftData> buffer;
  size_t last_insert_index = 0;
  size_t next_read_index = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_FFT_BUFFER_H_
