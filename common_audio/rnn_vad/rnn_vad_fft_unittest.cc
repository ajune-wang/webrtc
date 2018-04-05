/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>

#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/rnn_vad_fft.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "common_audio/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using rnn_vad::BiQuadFilter;
using rnn_vad::kHpfConfig48kHz;
using rnn_vad::RnnVadFft;
using rnn_vad::SequenceBuffer;

namespace {

// TODO(alessiob): Move to test_utils.cc/h.
void CheckFftResult(rtc::ArrayView<const float> expected_real,
                    rtc::ArrayView<const float> expected_imag,
                    rtc::ArrayView<const std::complex<float>> computed,
                    const float abs_tolerance) {
  ASSERT_EQ(expected_real.size(), expected_imag.size());
  ASSERT_EQ(computed.size(), expected_real.size());
  for (size_t i = 0; i < computed.size(); ++i) {
    SCOPED_TRACE(i);
    // TODO(alessiob): EXPECT_FLOAT_EQ() failes, check why.
    EXPECT_NEAR(expected_real[i], computed[i].real(), abs_tolerance);
    EXPECT_NEAR(expected_imag[i], computed[i].imag(), abs_tolerance);
  }
}

}  // namespace

TEST(RnnVadTest, ComputeForwardFftBitExactness) {
  // PCM samples reader and buffers.
  auto samples_reader = CreatePreprocessedSamplesReader();
  const size_t num_frames = samples_reader.second;
  std::array<float, kFrameSize10ms48kHz> samples;
  rtc::ArrayView<float, kFrameSize10ms48kHz> samples_view(samples.data(),
                                                          kFrameSize10ms48kHz);
  // FFT ground truth reader and buffers.
  auto fft_coeffs_reader = CreateFftCoeffsReader(true);
  ASSERT_EQ(num_frames, fft_coeffs_reader.second);
  std::array<float, kFftNumCoeffs20ms48kHz> fft_coeffs_real;
  std::array<float, kFftNumCoeffs20ms48kHz> fft_coeffs_imag;
  // Init pipeline.
  SequenceBuffer<float, kFrameSize20ms48kHz, kFrameSize10ms48kHz> seq_buf(0.f);
  auto seq_buf_view = seq_buf.GetBufferView();
  RnnVadFft fft(kFrameSize20ms48kHz);
  std::array<std::complex<float>, kFftNumCoeffs20ms48kHz> computed_fft_coeffs;

  // Process frames.
  {
    FloatingPointExceptionObserver fpe_observer;

    for (size_t i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      // Read 10 ms audio frame and corresponding FFT coefficients.
      samples_reader.first->ReadChunk(samples_view);
      fft_coeffs_reader.first->ReadChunk(
          {fft_coeffs_real.data(), fft_coeffs_real.size()});
      fft_coeffs_reader.first->ReadChunk(
          {fft_coeffs_imag.data(), fft_coeffs_imag.size()});
      // Run pipeline and check output.
      seq_buf.Push(samples_view);
      fft.ForwardFft(seq_buf_view);
      fft.CopyOutput({computed_fft_coeffs});
      CheckFftResult({fft_coeffs_real}, {fft_coeffs_imag},
                     {computed_fft_coeffs}, 1e-4f);
    }
  }
}

}  // namespace test
}  // namespace webrtc
