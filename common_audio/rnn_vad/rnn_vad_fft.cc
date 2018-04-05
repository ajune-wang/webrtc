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

#include "common_audio/rnn_vad/common.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Compute the first half of the Vorbis window.
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
      half_window_(ComputeHalfVorbisWindow(
          rtc::CheckedDivExact(frame_size, static_cast<size_t>(2)))),
#ifndef USE_KISS_FFT
      fft_(RealFourier::Create(RealFourier::FftOrder(frame_size_))),
      fft_length_(RealFourier::FftLength(fft_->order())),
      num_fft_points_(RealFourier::ComplexLength(fft_->order())),
      input_buf_(1, fft_length_, RealFourier::kFftBufferAlignment),
      output_buf_(1, num_fft_points_, RealFourier::kFftBufferAlignment)
#else
      fft_length_(frame_size),
      num_fft_points_(frame_size / 2 + 1),
      fft_(frame_size)
#endif
{
#ifdef USE_KISS_FFT
  // Init buffers.
  input_buf_.resize(frame_size_);
  output_buf_.resize(frame_size_);
  for (size_t i = 0; i < frame_size_; ++i)
    input_buf_[i] = 0.f;
#else
  RTC_DCHECK_EQ(0, frame_size_ % 2);
  RTC_DCHECK_EQ(fft_length_ / 2 + 1, num_fft_points_);
  RTC_DCHECK_EQ(half_window_.size() * 2, frame_size_);
  std::memset(input_buf_.Row(0), 0.f, input_buf_.cols() * sizeof(float));
#endif
}

RnnVadFft::~RnnVadFft() = default;

#ifdef USE_KISS_FFT
void RnnVadFft::ForwardFft(rtc::ArrayView<const float> samples) {
  RTC_DCHECK_EQ(frame_size_, samples.size());
  // Apply windowing.
  for (size_t i = 0; i < half_window_.size(); ++i) {
    input_buf_[i].real(samples[i] * half_window_[i]);
    input_buf_[frame_size_ - i - 1].real(
        samples[frame_size_ - i - 1] * half_window_[i]);
  }
  fft_.ForwardFft(frame_size_, input_buf_.data(), frame_size_,
                  output_buf_.data());
}

void RnnVadFft::CopyOutput(rtc::ArrayView<std::complex<float>> dst) {
  RTC_DCHECK_EQ(num_fft_points_, dst.size());
  std::memcpy(dst.data(), output_buf_.data(),
              num_fft_points_ * sizeof(std::complex<float>));
}
#else
void RnnVadFft::ForwardFft(rtc::ArrayView<const float> samples) {
  RTC_DCHECK_EQ(frame_size_, samples.size());
  // Apply windowing.
  for (size_t i = 0; i < half_window_.size(); ++i) {
    input_buf_.Row(0)[i] = samples[i] * half_window_[i];
    input_buf_.Row(0)[frame_size_ - i - 1] =
        samples[frame_size_ - i - 1] * half_window_[i];
  }
  fft_->Forward(input_buf_.Row(0), output_buf_.Row(0));
}

void RnnVadFft::CopyOutput(rtc::ArrayView<std::complex<float>> dst) {
  RTC_DCHECK_EQ(num_fft_points_, dst.size());
  std::memcpy(dst.data(), output_buf_.Row(0),
              num_fft_points_ * sizeof(std::complex<float>));
}
#endif

}  // namespace rnn_vad
}  // namespace webrtc
