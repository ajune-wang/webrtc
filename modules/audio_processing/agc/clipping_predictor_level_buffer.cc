/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor_level_buffer.h"

#include <algorithm>
#include <cmath>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
constexpr int kMaxAllowedBufferLength = 100;
}

ClippingPredictorLevelBuffer::ClippingPredictorLevelBuffer(
    int buffer_max_length)
    : tail_(-1), size_(0), data_(std::max(0, buffer_max_length)) {
  if (buffer_max_length > kMaxAllowedBufferLength) {
    RTC_LOG(LS_WARNING) << "[agc]: ClippingPredictorLevelBuffer exceeds the "
                        << "maximum allowed length. Size:" << buffer_max_length;
  }
  RTC_DCHECK_GE(buffer_max_length, 0);
}

void ClippingPredictorLevelBuffer::Reset() {
  tail_ = -1;
  size_ = 0;
}

void ClippingPredictorLevelBuffer::Push(Level level) {
  tail_ = (++tail_ == BufferMaxLength()) ? 0 : tail_;
  size_ = std::min(++size_, BufferMaxLength());
  data_[tail_] = level;
}

absl::optional<ClippingPredictorLevelBuffer::Level>
ClippingPredictorLevelBuffer::ComputePartialMetrics(int delay,
                                                    int num_items) const {
  RTC_DCHECK_GE(delay, 0);
  RTC_DCHECK_LT(delay, BufferMaxLength());
  RTC_DCHECK_GT(num_items, 0);
  RTC_DCHECK_LE(num_items, BufferMaxLength());
  RTC_DCHECK_LE(delay + num_items, BufferMaxLength());
  if (delay + num_items > Size()) {
    return absl::nullopt;
  }
  absl::optional<float> average = ComputePartialAverage(delay, num_items);
  absl::optional<float> max = ComputePartialMax(delay, num_items);
  if (average.has_value() && max.has_value()) {
    Level level = {*average, *max};
    return level;
  }
  return absl::nullopt;
}

absl::optional<float> ClippingPredictorLevelBuffer::ComputePartialAverage(
    int delay,
    int num_items) const {
  RTC_DCHECK_GE(delay, 0);
  RTC_DCHECK_LT(delay, BufferMaxLength());
  RTC_DCHECK_GT(num_items, 0);
  RTC_DCHECK_LE(num_items, BufferMaxLength());
  RTC_DCHECK_LE(delay + num_items, BufferMaxLength());
  if (delay + num_items > Size()) {
    return absl::nullopt;
  }
  float sum = 0.f;
  for (int i = 0; i < num_items && i < Size(); ++i) {
    int idx = tail_ - delay - i;
    idx += (idx < 0) ? BufferMaxLength() : 0;
    sum += data_[idx].average;
  }
  return sum / static_cast<float>(num_items);
}

absl::optional<float> ClippingPredictorLevelBuffer::ComputePartialMax(
    int delay,
    int num_items) const {
  RTC_DCHECK_GE(delay, 0);
  RTC_DCHECK_LT(delay, BufferMaxLength());
  RTC_DCHECK_GT(num_items, 0);
  RTC_DCHECK_LE(num_items, BufferMaxLength());
  RTC_DCHECK_LE(delay + num_items, BufferMaxLength());
  if (delay + num_items > Size()) {
    return absl::nullopt;
  }
  float max = 0.f;
  for (int i = 0; i < num_items && i < Size(); ++i) {
    int idx = tail_ - delay - i;
    idx += (idx < 0) ? BufferMaxLength() : 0;
    max = std::fmax(data_[idx].max, max);
  }
  return max;
}

}  // namespace webrtc
