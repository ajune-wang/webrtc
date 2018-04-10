/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_KISS_FFT_H_
#define COMMON_AUDIO_RNN_VAD_KISS_FFT_H_

#include <array>
#include <complex>
#include <vector>

#include "rtc_base/basictypes.h"

namespace webrtc {
namespace rnn_vad {

class KissFft {
 public:
  // Example: an FFT of length 128 has 4 factors as far as kissfft is concerned
  // (namely, 4*4*4*2).
  static const size_t kMaxFactors = 8;

  class KissFftState {
   public:
    explicit KissFftState(int num_fft_points);
    KissFftState(const KissFftState&) = delete;
    KissFftState& operator=(const KissFftState&) = delete;
    ~KissFftState();

    const int nfft;
    const float scale;
    std::array<int16_t, 2 * kMaxFactors> factors;
    std::vector<int16_t> bitrev;
    std::vector<std::complex<float>> twiddles;
  };

  explicit KissFft(const int nfft);
  KissFft(const KissFft&) = delete;
  KissFft& operator=(const KissFft&) = delete;
  ~KissFft();
  void ForwardFft(const size_t in_size,
                  const std::complex<float>* in,
                  const size_t out_size,
                  std::complex<float>* out);
  void ReverseFft(const size_t in_size,
                  const std::complex<float>* in,
                  const size_t out_size,
                  std::complex<float>* out);

 private:
  KissFftState state_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_KISS_FFT_H_
