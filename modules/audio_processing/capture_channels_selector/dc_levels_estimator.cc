/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/dc_levels_estimator.h"

#include <math.h>

#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

constexpr int kNumChannelsToReserve = 2;

}  // namespace

DcLevelsEstimator::DcLevelsEstimator() {
  dc_levels_.reserve(kNumChannelsToReserve);
}

bool DcLevelsEstimator::Update(const AudioBuffer& audio_buffer) {
  RTC_DCHECK_EQ(dc_levels_.size(), audio_buffer.num_channels());
  ++num_analyzed_frames_;
  for (size_t channel = 0; channel < audio_buffer.num_channels(); ++channel) {
    rtc::ArrayView<const float> channel_data(
        &audio_buffer.channels_const()[channel][0], audio_buffer.num_frames());
    RTC_DCHECK_EQ(channel_data.size(), num_samples_per_channel_);

    const float mean =
        std::accumulate(channel_data.begin(), channel_data.end(), 0.0f) *
        one_by_num_samples_per_channel_;

    constexpr float kForgettingFactor = 0.05f;
    dc_levels_[channel] += kForgettingFactor * (mean - dc_levels_[channel]);
  }

    // Empirical threshold for the number of frames that has to be analyzed for a
  // sufficiently reliable estimate to be obtained.
  constexpr int kNumFramesAnalyzedForReliableEstimates = 100;
  return num_analyzed_frames_ >= kNumFramesAnalyzedForReliableEstimates;
}

void DcLevelsEstimator::Reset() {
  num_analyzed_frames_ = 0;
  std::fill(dc_levels_.begin(), dc_levels_.end(), 0.0f);
}

void DcLevelsEstimator::SetAudioProperties(const AudioBuffer& audio_buffer) {
  dc_levels_.resize(audio_buffer.num_channels(), 0.0f);

  num_samples_per_channel_ = audio_buffer.num_frames();
  one_by_num_samples_per_channel_ = 1.0f / num_samples_per_channel_;
}

}  // namespace webrtc
