/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_BUFFER_STATISTICS_H_
#define MODULES_AUDIO_PROCESSING_AEC3_BUFFER_STATISTICS_H_

#include <vector>

namespace webrtc {

// Holds the circular buffer of the downsampled render data.
class BufferStatistics {
 public:
  BufferStatistics();
  ~BufferStatistics();
  void Reset();
  void AddUnderrun(size_t timestamp);
  void AddSurplusOverflow(size_t timestamp);

 private:
  std::vector<size_t> render_underruns_;
  std::vector<size_t> render_surplus_overflows_;
  int next_underrun_index_ = 0;
  int next_overflow_index_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_BUFFER_STATISTICS_H_
