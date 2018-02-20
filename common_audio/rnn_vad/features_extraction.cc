/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/features_extraction.h"

#include <cmath>
#include <iostream>

namespace webrtc {
namespace rnn_vad {
namespace {

const double kPi = std::acos(-1);

template <size_t S>
std::array<float, S> ComputeHalfVorbisWindow() {
  std::array<float, S> half_window;
  for (size_t i = 0; i < S; ++i)
    half_window[i] = std::sin(0.5 * kPi * std::sin(0.5 * kPi * (i + 0.5) / S) *
                              std::sin(0.5 * kPi * (i + 0.5) / S));
  return half_window;
}

// TODO(alessiob): replace with ArrayViewWithStep.
// // Decimate the pitch buffer without any anti-aliasing filter.
// void DecimateForPitchDetection(
//     rtc::ArrayView<float, kPitchBufferSize / 2> dst,
//     rtc::ArrayView<const float, kPitchBufferSize> src) {
//   for (size_t i = 1; i < rnn_vad::kAudioFrameSize; ++i)
//     dst[i] = src[2*i];
// }

}  // namespace

RnnVadFeaturesExtractor::RnnVadFeaturesExtractor()
    : analysis_buf_(0.f),
      analysis_win_(ComputeHalfVorbisWindow<kAudioFrameSize>()),
      fft_(RealFourier::Create(RealFourier::FftOrder(kAudioFrameBufferSize))),
      fft_input_buf_(1,
                     kAudioFrameBufferSize,
                     RealFourier::kFftBufferAlignment),
      fft_output_buf_(1,
                      RealFourier::ComplexLength(fft_->order()),
                      RealFourier::kFftBufferAlignment),
      is_silence_(true) {
  Reset();
}

RnnVadFeaturesExtractor::~RnnVadFeaturesExtractor() = default;

void RnnVadFeaturesExtractor::Reset() {
  feature_vector_.fill(0.f);
}

rtc::ArrayView<const float, kFeatureVectorSize>
RnnVadFeaturesExtractor::GetOutput() const {
  return {feature_vector_.data(), kFeatureVectorSize};
}

void RnnVadFeaturesExtractor::ComputeFeatures(
    rtc::ArrayView<const float, kAudioFrameSize> samples) {
  analysis_buf_.Push(samples);
  AnalyzeFrame();
  is_silence_ = false;
}

void RnnVadFeaturesExtractor::AnalyzeFrame() {
  // Windowing.
  const auto samples = analysis_buf_.GetBufferView(kPitchMaxPeriod);
  for (size_t i = 0; i < kAudioFrameSize; ++i) {
    fft_input_buf_.Row(0)[i] = analysis_win_[i] * samples[i];
    fft_input_buf_.Row(0)[kAudioFrameBufferSize - i - 1] =
        analysis_win_[i] * samples[kAudioFrameBufferSize - i - 1];
  }
  // FFT.
  fft_->Forward(fft_input_buf_.Row(0), fft_output_buf_.Row(0));
  // Compute band energy.
  // TODO(alessiob): Implement.
}

}  // namespace rnn_vad
}  // namespace webrtc
