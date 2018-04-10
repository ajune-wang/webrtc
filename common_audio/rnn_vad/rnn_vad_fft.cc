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

std::array<float, impl::kMaxFftHalfWinSize> ComputeHalfVorbisWindow(
    const size_t size) {
  RTC_DCHECK_LT(0, size);
  RTC_DCHECK_LE(size, impl::kMaxFftHalfWinSize);
  std::array<float, impl::kMaxFftHalfWinSize> half_window{};
  for (size_t i = 0; i < size; ++i)
    half_window[i] =
        std::sin(0.5 * kPi * std::sin(0.5 * kPi * (i + 0.5) / size) *
                 std::sin(0.5 * kPi * (i + 0.5) / size));
  return half_window;
}

RnnVadFft::RnnVadFft(size_t frame_size)
    : frame_size_(frame_size),
      half_window_(ComputeHalfVorbisWindow(
          rtc::CheckedDivExact(frame_size, static_cast<size_t>(2)))),
      fft_length_(frame_size),
      num_fft_points_(frame_size / 2 + 1),
      fft_(frame_size) {
  RTC_CHECK((frame_size_ & 1) == 0) << "The frame size must be even.";
  RTC_CHECK_LE(frame_size_, input_buf_.size());
  for (size_t i = 0; i < frame_size_; ++i)
    input_buf_[i] = 0.f;
}

RnnVadFft::~RnnVadFft() = default;

void RnnVadFft::ForwardFft(rtc::ArrayView<const float> samples,
                           rtc::ArrayView<std::complex<float>> dst) {
  RTC_DCHECK_EQ(frame_size_, samples.size());
  RTC_DCHECK_EQ(fft_length_, dst.size());
  // Apply windowing.
  for (size_t i = 0; i < frame_size_ / 2; ++i) {
    input_buf_[i].real(samples[i] * half_window_[i]);
    input_buf_[frame_size_ - i - 1].real(samples[frame_size_ - i - 1] *
                                         half_window_[i]);
  }
  fft_.ForwardFft(frame_size_, input_buf_.data(), frame_size_, dst.data());
}

}  // namespace rnn_vad
}  // namespace webrtc
