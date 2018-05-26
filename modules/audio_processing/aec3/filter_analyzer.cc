/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/filter_analyzer.h"
#include <math.h>

#include <algorithm>
#include <array>
#include <numeric>

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

size_t FindPeakIndex(rtc::ArrayView<const float> filter_time_domain) {
  size_t peak_index = 0;
  float max_h2 = filter_time_domain[0] * filter_time_domain[0];
  for (size_t k = 1; k < filter_time_domain.size(); ++k) {
    float tmp = filter_time_domain[k] * filter_time_domain[k];
    if (tmp > max_h2) {
      peak_index = k;
      max_h2 = tmp;
    }
  }

  return peak_index;
}

const CascadedBiQuadFilter::BiQuadCoefficients kHighPassFilterCoefficients = {
    {0.94598f, -1.89195f, 0.94598f},
    {-1.88903f, 0.89487f}};

}  // namespace

int FilterAnalyzer::instance_count_ = 0;

FilterAnalyzer::FilterAnalyzer(const EchoCanceller3Config& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      bounded_erl_(config.ep_strength.bounded_erl),
      default_gain_(config.ep_strength.lf),
      active_render_threshold_(config.render_levels.active_render_limit *
                               config.render_levels.active_render_limit *
                               kFftLengthBy2),
      hp_analysis_step_size_(
          config.echo_removal_control.filter_analyzer_step_size),
      h_highpass_(GetTimeDomainLength(config.filter.main.length_blocks), 0.f),
      hp_filter_(kHighPassFilterCoefficients, 1) {
  Reset();
}

bool FilterAnalyzer::PreProcessFilter(
    rtc::ArrayView<const float> filter_time_domain) {
  RTC_DCHECK_GE(h_highpass_.capacity(), filter_time_domain.size());
  if (h_highpass_.size() > 0) {
    h_highpass_.resize(std::min(h_highpass_.size(), filter_time_domain.size()));
    h_highpass_index_ = std::min(h_highpass_index_, h_highpass_.size());
  }

  h_highpass_.resize(std::min(h_highpass_.size() + hp_analysis_step_size_,
                              filter_time_domain.size()));

  if (h_highpass_index_ == 0) {
    hp_filter_.Reset();
  }

  size_t start = h_highpass_index_;
  size_t length = std::min(hp_analysis_step_size_, h_highpass_.size() - start);
  RTC_DCHECK_GE(h_highpass_.size(), start + length);
  hp_filter_.Process(
      rtc::ArrayView<const float>(&filter_time_domain[start], length),
      rtc::ArrayView<float>(&h_highpass_[start], length));

  h_highpass_index_ += length;
  RTC_DCHECK_GE(h_highpass_.size(), h_highpass_index_);
  if (h_highpass_index_ == h_highpass_.size() - 1) {
    h_highpass_index_ = 0;
  }

  return h_highpass_.size() < filter_time_domain.size() ||
         h_highpass_index_ == 0;
}

FilterAnalyzer::~FilterAnalyzer() = default;

void FilterAnalyzer::Reset() {
  delay_blocks_ = 0;
  consistent_estimate_ = false;
  blocks_since_reset_ = 0;
  consistent_estimate_ = false;
  consistent_estimate_counter_ = 0;
  consistent_delay_reference_ = -10;
  gain_ = default_gain_;
  h_highpass_.resize(0);
  h_highpass_index_ = 0;
}

void FilterAnalyzer::Update(rtc::ArrayView<const float> filter_time_domain,
                            const RenderBuffer& render_buffer) {
  // Preprocess the filter to avoid low-frequency components disturbing the
  // filter
  PreProcessFilter(filter_time_domain);

  size_t peak_index = FindPeakIndex(h_highpass_);
  delay_blocks_ = peak_index >> kBlockSizeLog2;
  UpdateFilterGain(h_highpass_, peak_index);

  bool fully_preprocessed_filter =
      h_highpass_.size() == filter_time_domain.size();
  if (fully_preprocessed_filter) {
    RTC_DCHECK_EQ(h_highpass_.size(), filter_time_domain.size());
    float filter_floor = 0;
    float filter_secondary_peak = 0;
    size_t limit1 = peak_index < 64 ? 0 : peak_index - 64;
    size_t limit2 =
        peak_index > h_highpass_.size() - 129 ? 0 : peak_index + 128;

    for (size_t k = 0; k < limit1; ++k) {
      float abs_h = fabsf(h_highpass_[k]);
      filter_floor += abs_h;
      filter_secondary_peak = std::max(filter_secondary_peak, abs_h);
    }
    for (size_t k = limit2; k < h_highpass_.size(); ++k) {
      float abs_h = fabsf(h_highpass_[k]);
      filter_floor += abs_h;
      filter_secondary_peak = std::max(filter_secondary_peak, abs_h);
    }

    filter_floor /= (limit1 + h_highpass_.size() - limit2);

    float abs_peak = fabsf(h_highpass_[peak_index]);
    bool significant_peak_index = abs_peak > 10.f * filter_floor &&
                                  abs_peak > 2.f * filter_secondary_peak;

    if (consistent_delay_reference_ != delay_blocks_ ||
        !significant_peak_index) {
      consistent_estimate_counter_ = 0;
      consistent_delay_reference_ = delay_blocks_;
    } else {
      const auto& x = render_buffer.Block(-delay_blocks_)[0];
      const float x_energy =
          std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
      const bool active_render_block = x_energy > active_render_threshold_;

      if (active_render_block) {
        ++consistent_estimate_counter_;
      }
    }

    consistent_estimate_ =
        consistent_estimate_counter_ > 1.5f * kNumBlocksPerSecond;
  }

  size_t current_size = h_highpass_.size();
  h_highpass_.resize(h_highpass_.capacity());
  data_dumper_->DumpRaw("aec3_linear_filter_processed_td", h_highpass_);
  h_highpass_.resize(current_size);
}

void FilterAnalyzer::UpdateFilterGain(
    rtc::ArrayView<const float> filter_time_domain,
    size_t peak_index) {
  bool sufficient_time_to_converge =
      ++blocks_since_reset_ > 5 * kNumBlocksPerSecond;

  if (sufficient_time_to_converge && consistent_estimate_) {
    gain_ = fabsf(filter_time_domain[peak_index]);
  } else {
    if (gain_) {
      gain_ = std::max(gain_, fabsf(filter_time_domain[peak_index]));
    }
  }

  if (bounded_erl_ && gain_) {
    gain_ = std::max(gain_, 0.01f);
  }
}

}  // namespace webrtc
