/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_SEQUENCE_BUFFER_H_
#define COMMON_AUDIO_RNN_VAD_SEQUENCE_BUFFER_H_

#include <array>

namespace webrtc {
namespace rnn_vad {

// A sequence buffer provides a view on the last S samples of a sequence which
// is read in chunks of N samples. For instance, when S = 2N half of the first
// half of the sequence buffer is replaced with its second half and the new N
// items are written at the end of the buffer.
template <typename T, size_t S, size_t N, typename D = T>
class SequenceBuffer {
  static_assert(S >= N,
                "The new chunk size is larger than the sequence buffer size.");

 public:
  explicit SequenceBuffer(const T& init_value) { buffer_.fill(init_value); }
  ~SequenceBuffer() = default;
  size_t size() const { return S; }
  size_t chunks_size() const { return N; }
  rtc::ArrayView<const T, S> GetBufferView() const {
    return {buffer_.data(), S};
  }
  void Push(rtc::ArrayView<const T, N> new_values) {
    // Make space for the new values.
    if (S > N)
      std::memmove(buffer_.data(), buffer_.data() + N, S - N);
    // Copy the new values at the end of the buffer.
    std::memcpy(buffer_.data() + S - N, new_values.data(), N);
  }
  void CopyBuffer(rtc::ArrayView<T> dst) {
    RTC_CHECK_EQ(S, dst.size());
    std::memcpy(dst.data(), buffer_.data(), S);
  }

 private:
  std::array<T, S> buffer_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_SEQUENCE_BUFFER_H_
