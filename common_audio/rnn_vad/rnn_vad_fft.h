/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_RNN_VAD_FFT_H_
#define COMMON_AUDIO_RNN_VAD_RNN_VAD_FFT_H_

#include <array>
#include <complex>
#include <memory>

#include "api/array_view.h"
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/kiss_fft.h"

namespace webrtc {
namespace rnn_vad {
namespace impl {

// FFT input buffer over-allocation used to exploit arrays and avoid dynamic
// allocation.
constexpr size_t kMaxFftFrameSize = 1024;
static_assert((kMaxFftFrameSize & 1) == 0, "kMaxFftFrameSize must be even.");
constexpr size_t kMaxFftHalfWinSize = kMaxFftFrameSize / 2;

}  // namespace impl

// Compute the first half of the Vorbis window.
std::array<float, impl::kMaxFftHalfWinSize> ComputeHalfVorbisWindow(
    const size_t size);

// FFT wrapper for the RNN VAD which holds buffers and provides an interface
// to an FFT implementation.
class RnnVadFft {
 public:
  explicit RnnVadFft(size_t frame_size);
  RnnVadFft(const RnnVadFft&) = delete;
  RnnVadFft& operator=(const RnnVadFft&) = delete;
  ~RnnVadFft();
  size_t fft_length() const { return fft_length_; }
  size_t num_fft_points() const { return num_fft_points_; }
  // Applies a windowing function to |samples|, computes the real forward FFT
  // and writes the result in |dst|.
  void ForwardFft(rtc::ArrayView<const float> samples,
                  rtc::ArrayView<std::complex<float>> dst);

 private:
  const size_t frame_size_;
  const std::array<float, impl::kMaxFftHalfWinSize> half_window_;
  const size_t fft_length_;
  const size_t num_fft_points_;
  KissFft fft_;
  std::array<std::complex<float>, impl::kMaxFftFrameSize> input_buf_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_RNN_VAD_FFT_H_
