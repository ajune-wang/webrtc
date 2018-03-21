/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/filter_based_delay_estimator.h"

#include <math.h>

#include <algorithm>
#include <array>
#include <numeric>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

// TODO(peah): Generalize and merge with the counterpart in subtractor.cc.
void PredictionError(const Aec3Fft& fft,
                     const FftData& S,
                     rtc::ArrayView<const float> y,
                     std::array<float, kBlockSize>* e,
                     std::array<float, kBlockSize>* s,
                     bool* saturation) {
  std::array<float, kFftLength> tmp;
  fft.Ifft(S, &tmp);
  constexpr float kScale = 1.0f / kFftLengthBy2;
  std::transform(y.begin(), y.end(), tmp.begin() + kFftLengthBy2, e->begin(),
                 [&](float a, float b) { return a - b * kScale; });

  *saturation = false;

  if (s) {
    for (size_t k = 0; k < s->size(); ++k) {
      (*s)[k] = kScale * tmp[k + kFftLengthBy2];
    }
    auto result = std::minmax_element(s->begin(), s->end());
    *saturation = *result.first <= -32768 || *result.first >= 32767;
  }
  if (!(*saturation)) {
    auto result = std::minmax_element(e->begin(), e->end());
    *saturation = *result.first <= -32768 || *result.first >= 32767;
  }

  std::for_each(e->begin(), e->end(),
                [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
}

}  // namespace

FilterBasedDelayEstimator::FilterBasedDelayEstimator(
    const EchoCanceller3Config& config,
    ApmDataDumper* data_dumper,
    Aec3Optimization optimization)
    : fft_(),
      optimization_(optimization),
      config_(config),
      filter_(config_.filter.echo_remover_delay_filter.length_blocks,
              config_.filter.echo_remover_delay_filter.length_blocks,
              config_.filter.config_change_duration_blocks,
              optimization,
              data_dumper),
      G_filter_(config_.filter.echo_remover_delay_filter,
                config_.filter.config_change_duration_blocks),
      filter_analyzer_(config_) {}

FilterBasedDelayEstimator::~FilterBasedDelayEstimator() = default;

void FilterBasedDelayEstimator::Reset() {
  filter_.HandleEchoPathChange();
  G_filter_.HandleEchoPathChange();
  rtc::Optional<int> delay_blocks_ = rtc::nullopt;
}

void FilterBasedDelayEstimator::Update(
    const RenderBuffer& render_buffer,
    const rtc::ArrayView<const float> capture,
    const RenderSignalAnalyzer& render_signal_analyzer,
    const AecState& aec_state) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());
  rtc::ArrayView<const float> y = capture;
  FftData E;
  std::array<float, kBlockSize> e;
  FftData S;
  FftData& G = S;
  bool saturation = false;
  std::array<float, kFftLengthBy2Plus1> E2;

  filter_.Filter(render_buffer, &S);
  PredictionError(fft_, S, y, &e, nullptr, &saturation);
  fft_.ZeroPaddedFft(e, Aec3Fft::Window::kHanning, &E);

  E.Spectrum(optimization_, E2);

  std::array<float, kFftLengthBy2Plus1> X2;
  render_buffer.SpectralSum(filter_.SizePartitions(), &X2);
  G_filter_.Compute(X2, render_signal_analyzer, E, filter_.SizePartitions(),
                    aec_state.SaturatedCapture() || saturation, &G);

  filter_.Adapt(render_buffer, G);
  const auto& impulse_response = filter_.FilterImpulseResponse();

  filter_analyzer_.Update(impulse_response, render_buffer);

  delay_blocks_ = filter_analyzer_.Consistent()
                      ? rtc::Optional<int>(filter_analyzer_.DelayBlocks())
                      : rtc::nullopt;

  DumpFilter();
}

}  // namespace webrtc
