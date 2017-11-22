/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/buffer_statistics.h"

namespace webrtc {
namespace {
constexpr size_t kStatBufferSize = 100;

}  // namespace

BufferStatistics::BufferStatistics()
    : render_underruns_(kStatBufferSize, 0),
      render_surplus_overflows_(kStatBufferSize, 0) {
  Reset();
}

void BufferStatistics::Reset() {
  render_underruns_.resize(0);
  render_surplus_overflows_.resize(0);
}

void BufferStatistics::AddUnderrun(size_t timestamp) {
  if (render_underruns_.size() < kStatBufferSize) {
    render_underruns_.resize(render_underruns_.size() + 1);
  }
  render_underruns_[next_underrun_index_] = timestamp;
  next_underrun_index_ = (next_underrun_index_ + 1) % render_underruns_.size();
}

void BufferStatistics::AddSurplusOverflow(size_t timestamp) {
  if (render_surplus_overflows_.size() < kStatBufferSize) {
    render_surplus_overflows_.resize(render_surplus_overflows_.size() + 1);
  }
  render_surplus_overflows_[next_overflow_index_] = timestamp;
  next_overflow_index_ =
      (next_overflow_index_ + 1) % render_surplus_overflows_.size();
}

BufferStatistics::~BufferStatistics() = default;

}  // namespace webrtc
