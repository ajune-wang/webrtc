/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/alignment_mixer.h"

#include <algorithm>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

AlignmentMixer::AlignmentVariant ChooseAlignmentVariant(bool downmix,
                                                        bool adaptive_selection,
                                                        int num_channels) {
  RTC_DCHECK(!(adaptive_selection && downmix));
  RTC_DCHECK_LT(0, num_channels);

  if (num_channels == 1) {
    return AlignmentMixer::AlignmentVariant::kFixed;
  }
  if (downmix) {
    return AlignmentMixer::AlignmentVariant::kDownmix;
  }
  if (adaptive_selection) {
    return AlignmentMixer::AlignmentVariant::kAdaptive;
  }
  return AlignmentMixer::AlignmentVariant::kFixed;
}

}  // namespace

AlignmentMixer::AlignmentMixer(
    size_t num_channels,
    const EchoCanceller3Config::Delay::AlignmentMixing& config)
    : AlignmentMixer(num_channels,
                     config.downmix,
                     config.adaptive_selection,
                     config.excitation_limit,
                     config.prefer_first_two_channels) {}

AlignmentMixer::AlignmentMixer(size_t num_channels,
                               bool downmix,
                               bool adaptive_selection,
                               float excitation_limit,
                               bool prefer_first_two_channels)
    : num_channels_(num_channels),
      one_by_num_channels_(1.f / num_channels_),
      excitation_limit_(kBlockSize * excitation_limit * excitation_limit),
      prefer_first_two_channels_(prefer_first_two_channels),
      selection_variant_(
          ChooseAlignmentVariant(downmix, adaptive_selection, num_channels_)) {
  if (selection_variant_ == AlignmentVariant::kAdaptive) {
    strong_block_counters_.resize(2);
    std::fill(strong_block_counters_.begin(), strong_block_counters_.end(), 0);
    cumulative_energies_.resize(num_channels_);
    std::fill(cumulative_energies_.begin(), cumulative_energies_.end(), 0.f);
  }
}

void AlignmentMixer::ProduceOutput(rtc::ArrayView<const std::vector<float>> x,
                                   rtc::ArrayView<float, kBlockSize> y) {
  RTC_DCHECK_EQ(x.size(), num_channels_);
  if (selection_variant_ == AlignmentVariant::kDownmix) {
    Downmix(x, y);
    return;
  }

  int ch =
      selection_variant_ == AlignmentVariant::kFixed ? 0 : SelectChannel(x);

  RTC_DCHECK_GE(x.size(), ch);
  std::copy(x[ch].begin(), x[ch].end(), y.begin());
}

void AlignmentMixer::Downmix(rtc::ArrayView<const std::vector<float>> x,
                             rtc::ArrayView<float, kBlockSize> y) const {
  RTC_DCHECK_EQ(x.size(), num_channels_);
  RTC_DCHECK_GE(num_channels_, 2);
  std::copy(x[0].begin(), x[0].end(), y.begin());
  for (size_t ch = 1; ch < num_channels_; ++ch) {
    for (size_t i = 0; i < kBlockSize; ++i) {
      y[i] += x[ch][i];
    }
  }

  for (size_t i = 0; i < kBlockSize; ++i) {
    y[i] *= one_by_num_channels_;
  }
}

int AlignmentMixer::SelectChannel(rtc::ArrayView<const std::vector<float>> x) {
  RTC_DCHECK_EQ(x.size(), num_channels_);
  RTC_DCHECK_GE(num_channels_, 2);
  RTC_DCHECK_EQ(strong_block_counters_.size(), 2);
  RTC_DCHECK_EQ(cumulative_energies_.size(), num_channels_);

  constexpr size_t kBlocksToChooseLeftOrRight =
      static_cast<size_t>(0.5f * kNumBlocksPerSecond);
  bool chosen_left_or_right =
      prefer_first_two_channels_ &&
      (strong_block_counters_[0] > kBlocksToChooseLeftOrRight ||
       strong_block_counters_[1] > kBlocksToChooseLeftOrRight);

  const int num_ch_to_analyze = chosen_left_or_right ? 2 : num_channels_;

  for (int ch = 0; ch < num_ch_to_analyze; ++ch) {
    RTC_DCHECK_EQ(x[ch].size(), kBlockSize);
    float x2_sum = 0.f;
    for (size_t i = 0; i < kBlockSize; ++i) {
      x2_sum += x[ch][i] * x[ch][i];
    }

    if (ch < 2 && x2_sum > excitation_limit_) {
      ++strong_block_counters_[ch];
    }

    cumulative_energies_[ch] += x2_sum;
  }

  int strongest_ch = 0;
  for (int ch = 0; ch < num_ch_to_analyze; ++ch) {
    if (cumulative_energies_[ch] > cumulative_energies_[strongest_ch]) {
      strongest_ch = ch;
    }
  }

  if (selected_channel_ > 1 ||
      cumulative_energies_[strongest_ch] >
          2.f * cumulative_energies_[selected_channel_]) {
    selected_channel_ = strongest_ch;
  }

  return selected_channel_;
}

}  // namespace webrtc
