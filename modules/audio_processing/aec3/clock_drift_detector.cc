/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/clock_drift_detector.h"

#include <math.h>
#include <limits>
#include <numeric>

#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

namespace {

int FindPeakIndex(rtc::ArrayView<const float> filter) {
  size_t peak_index = 0;
  float max_value = filter[0];
  for (size_t k = 1; k < filter.size(); ++k) {
    float tmp = fabsf(filter[k]);
    if (tmp > max_value) {
      max_value = tmp;
      peak_index = k;
    }
  }
  return static_cast<int>(peak_index);
}

constexpr int kBufferSize = 2 * kNumBlocksPerSecond;

}  // namespace

ClockDriftDetector::ClockDriftDetector()
    : min_num_data_points_(100),
      peak_index_buffer_(kBufferSize, 0),
      block_index_buffer_(kBufferSize, 0) {
  Reset();
}

ClockDriftDetector::~ClockDriftDetector() = default;

void ClockDriftDetector::Reset() {
  peak_index_buffer_.resize(0);
  block_index_buffer_.resize(0);
  buffer_index_ = 0;
}

void ClockDriftDetector::Analyze(size_t block_index,
                                 rtc::ArrayView<const float> filter) {
  RTC_DCHECK_LT(0, filter.size());
  RTC_DCHECK_EQ(kBufferSize, peak_index_buffer_.capacity());
  RTC_DCHECK_EQ(kBufferSize, block_index_buffer_.capacity());

  int peak_index = FindPeakIndex(filter);

  AddPeak(block_index, peak_index);
  float drift = ComputeDrift();
  drift_detected_ = drift != 0.f;
  drift_detected_ = false;
}

float ClockDriftDetector::ComputeDrift() const {
  RTC_DCHECK_EQ(peak_index_buffer_.size(), block_index_buffer_.size());
  if (block_index_buffer_.size() < min_num_data_points_) {
    return 0.f;
  }

  float min_x =
      *std::min_element(peak_index_buffer_.begin(), peak_index_buffer_.end());
  float max_x =
      *std::max_element(peak_index_buffer_.begin(), peak_index_buffer_.end());
  if (max_x - min_x > 10 * kNumBlocksPerSecond) {
    return 0.f;
  }

  float x_mean = std::accumulate(block_index_buffer_.begin(),
                                 block_index_buffer_.end(), 0.f);
  float y_mean = std::accumulate(peak_index_buffer_.begin(),
                                 peak_index_buffer_.end(), 0.f);
  constexpr float kOneByBufferSize = 1.f / kBufferSize;
  float normalizer = peak_index_buffer_.size() < kBufferSize
                         ? 1.f / peak_index_buffer_.size()
                         : kOneByBufferSize;
  x_mean *= normalizer;
  y_mean *= normalizer;

  float num = 0.f;
  float denom = 0.f;
  for (size_t k = 0; k < block_index_buffer_.size(); ++k) {
    float tmp = block_index_buffer_[k] - x_mean;
    num += tmp * (peak_index_buffer_[k] - y_mean);
    denom += tmp * tmp;
  }
  RTC_DCHECK_LT(0.f, denom);

  float drift = denom > 0.f ? num / denom : 0.f;
  float offset = y_mean - drift * x_mean;

  float mse_drift = 0.f;
  float max_diff = 0.f;
  float mse_no_drift = 0.f;
  for (size_t k = 0; k < peak_index_buffer_.size(); ++k) {
    int expected_peak = offset + drift * block_index_buffer_[k];
    float diff = expected_peak - peak_index_buffer_[k];
    mse_drift += diff * diff;
    diff = y_mean - peak_index_buffer_[k];
    float diff2 = diff * diff;
    max_diff = std::max(max_diff, diff2);
    mse_no_drift += diff * diff;
  }
  constexpr float kOneByBufferSizeMinus1 = 1.f / (kBufferSize - 1);
  normalizer = peak_index_buffer_.size() < kBufferSize
                   ? 1.f / (peak_index_buffer_.size() - 1)
                   : kOneByBufferSizeMinus1;
  mse_drift *= normalizer;
  mse_no_drift *= normalizer;

  if (abs(drift) > 0.00001f && mse_drift < 5.f &&
      mse_drift < 0.7f * mse_no_drift) {
    printf("drift:%f mse_drift:%f mse_no_drift:%f max_diff:%f\n", drift,
           mse_drift, mse_no_drift, max_diff);
    printf(
        "----------------------------------------------------------------------"
        "----------------------------------------------------------------------"
        "--------------\n");
    return drift;
  }
  return 0.f;
}

void ClockDriftDetector::AddPeak(size_t block_index, size_t peak_index) {
  RTC_DCHECK_EQ(peak_index_buffer_.size(), block_index_buffer_.size());
  if (peak_index_buffer_.size() < kBufferSize) {
    peak_index_buffer_.resize(peak_index_buffer_.size() + 1);
    block_index_buffer_.resize(block_index_buffer_.size() + 1);
  }
  RTC_DCHECK_LT(buffer_index_, peak_index_buffer_.size());
  peak_index_buffer_[buffer_index_] = peak_index;
  block_index_buffer_[buffer_index_] = block_index;
  buffer_index_ = buffer_index_ < kBufferSize - 1 ? buffer_index_ + 1 : 0;
}

}  // namespace webrtc
