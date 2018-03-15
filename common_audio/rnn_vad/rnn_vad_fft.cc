/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/rnn_vad_fft.h"

#include <cmath>
#include <iostream>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace rnn_vad {
namespace {

const double kPi = std::acos(-1);

// Computes the first half of the Vorbis window.
std::vector<float> ComputeHalfVorbisWindow(const size_t size) {
  RTC_DCHECK_LT(0, size);
  std::vector<float> half_window;
  half_window.reserve(size);
  for (size_t i = 0; i < size; ++i)
    half_window.push_back(std::sin(0.5 * kPi *
                                   std::sin(0.5 * kPi * (i + 0.5) / size) *
                                   std::sin(0.5 * kPi * (i + 0.5) / size)));
  return half_window;
}

}  // namespace

RnnVadFft::RnnVadFft(size_t frame_size)
    : frame_size_(frame_size),
      half_window_(ComputeHalfVorbisWindow(frame_size / 2)),
      fft_(RealFourier::Create(RealFourier::FftOrder(frame_size_))),
      fft_input_buf_(1, frame_size_, RealFourier::kFftBufferAlignment),
      fft_output_buf_(1,
                      RealFourier::ComplexLength(fft_->order()),
                      RealFourier::kFftBufferAlignment),
      fft_length_(RealFourier::FftLength(fft_->order())),
      num_fft_points_(fft_output_buf_.cols()) {
  RTC_DCHECK_EQ(0, frame_size_ % 2);
  RTC_DCHECK_EQ(fft_length_ / 2 + 1, num_fft_points_);
  RTC_DCHECK_EQ(half_window_.size() * 2, frame_size_);
  RTC_LOG(LS_INFO) << "RnnVadFft initialized (frame length: " << fft_length_
                   << ", # FFT points: " << num_fft_points_ << ")";
}

RnnVadFft::~RnnVadFft() = default;

rtc::ArrayView<const std::complex<float>> RnnVadFft::GetFftOutputBufferView()
    const {
  return {fft_output_buf_.Row(0), num_fft_points_};
}

void RnnVadFft::ForwardFft(rtc::ArrayView<const float> samples) {
  RTC_DCHECK_EQ(frame_size_, samples.size());
  // Apply windowing.
  for (size_t i = 0; i < half_window_.size(); ++i) {
    fft_input_buf_.Row(0)[i] = half_window_[i] * samples[i];
    fft_input_buf_.Row(0)[frame_size_ - i - 1] =
        half_window_[i] * samples[frame_size_ - i - 1];
  }
  // TODO(alessiob): Open a bug since it is not documented that the output
  // buffer must be cleaned by the caller.
  std::memset(fft_output_buf_.Row(0), 0,
              num_fft_points_ * sizeof(std::complex<float>));
  fft_->Forward(fft_input_buf_.Row(0), fft_output_buf_.Row(0));
}

}  // namespace rnn_vad
}  // namespace webrtc
