/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_MATRIX_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_MATRIX_BUFFER_H_

#include <vector>

namespace webrtc {
namespace aec3 {
struct MatrixBuffer {
  MatrixBuffer(size_t size, size_t height, size_t width);
  ~MatrixBuffer();
  void Clear();

  std::vector<std::vector<std::vector<float>>> buffer;
  size_t last_insert_index = 0;
  size_t next_read_index = 0;
};

}  // namespace aec3
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_MATRIX_BUFFER_H_
