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
#include <ostream>
#include <type_traits>

namespace webrtc {
namespace rnn_vad {

// A sequence buffer provides a view on the last S samples of a sequence which
// is read in chunks of N samples. For instance, when S = 2N the first half of
// the sequence buffer is replaced with its second half, and the new N items are
// written at the end of the buffer.
template <typename T, size_t S, size_t N>
class SequenceBuffer {
  static_assert(S >= N,
                "The new chunk size is larger than the sequence buffer size.");
  static_assert(std::is_arithmetic<T>::value,
                "Integral or floating point required.");

 public:
  explicit SequenceBuffer(const T& init_value) { buffer_.fill(init_value); }
  ~SequenceBuffer() = default;
  SequenceBuffer(const SequenceBuffer&) = delete;
  SequenceBuffer& operator=(const SequenceBuffer&) = delete;
  size_t size() const { return S; }
  size_t chunks_size() const { return N; }
  // Returns a view on the whole array; if an offset is given, the first element
  // starts at the given offset and the size is reduced accordingly.
  rtc::ArrayView<const T> GetBufferView(size_t offset = 0) const {
    RTC_DCHECK_LT(offset, S);
    return {buffer_.data() + offset, S - offset};
  }
  // Shifts left the buffer by N items and adds new N items at the end.
  void Push(rtc::ArrayView<const T, N> new_values) {
    // Make space for the new values.
    if (S > N)
      std::memmove(buffer_.data(), buffer_.data() + N, (S - N) * sizeof(T));
    // Copy the new values at the end of the buffer.
    std::memcpy(buffer_.data() + S - N, new_values.data(), N * sizeof(T));
  }

 private:
  std::array<T, S> buffer_;
};

template <typename T, size_t S, size_t N>
std::ostream& operator<<(std::ostream& os, const SequenceBuffer<T, S, N>& t) {
  auto buf_view = t.GetBufferView();
  os << "[" << S << ", " << N << "]";
  for (const auto v : buf_view)
    os << " " << v;
  return os;
}

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_SEQUENCE_BUFFER_H_
