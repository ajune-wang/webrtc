/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <ostream>

#include "api/array_view.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using rnn_vad::SequenceBuffer;

namespace {

template <typename T, size_t S, size_t N>
void TestSequenceBufferPushOp() {
  std::ostringstream ss;
  ss << "size: " << S << ", new: " << N;
  SCOPED_TRACE(ss.str());
  static_assert(std::is_integral<T>::value, "Integral required.");
  SequenceBuffer<T, S, N> seq_buf(0);
  auto seq_buf_view = seq_buf.GetBufferView();
  std::array<T, N> chunk;
  rtc::ArrayView<T, N> chunk_view(chunk.data(), N);

  // Check that a chunk is fully gone after ceil(S / N) push ops.
  chunk.fill(1);
  seq_buf.Push(chunk_view);
  chunk.fill(0);
  constexpr size_t required_push_ops = (S % N) ? S / N + 1 : S / N;
  // TODO(alessiob): Check why if the next line is remove the test fail when
  // NDEBUG.
  RTC_LOG(LS_INFO) << required_push_ops;
  for (size_t i = 0; i < required_push_ops - 1; ++i) {
    seq_buf.Push(chunk_view);
    // Still in the buffer.
    const auto* m = std::max_element(seq_buf_view.begin(), seq_buf_view.end());
    EXPECT_EQ(1, *m);
  }
  // Gone after another push.
  seq_buf.Push(chunk_view);
  const auto* m = std::max_element(seq_buf_view.begin(), seq_buf_view.end());
  EXPECT_EQ(0, *m);

  // Check that the last item moves left by N positions after a push op.
  for (T i = 0; i < N; ++i)
    chunk[i] = i + 1;
  seq_buf.Push(chunk_view);
  const T last = chunk[N - 1];
  for (T i = 0; i < N; ++i)
    chunk[i] = last + i + 1;
  seq_buf.Push(chunk_view);
}

}  // namespace

TEST(RnnVad, SequenceBufferGetters) {
  constexpr size_t buffer_size = 8;
  constexpr size_t chunk_size = 8;
  SequenceBuffer<uint8_t, buffer_size, chunk_size> seq_buf(0);
  EXPECT_EQ(buffer_size, seq_buf.size());
  EXPECT_EQ(chunk_size, seq_buf.chunks_size());
  // Test view.
  auto seq_buf_view = seq_buf.GetBufferView();
  EXPECT_EQ(0, *seq_buf_view.begin());
  EXPECT_EQ(0, *seq_buf_view.end());
  constexpr std::array<uint8_t, chunk_size> chunk = {10, 20, 30, 40,
                                                     50, 60, 70, 80};
  seq_buf.Push({chunk.data(), chunk_size});
  EXPECT_EQ(10, *seq_buf_view.begin());
  EXPECT_EQ(80, *(seq_buf_view.end() - 1));
}

TEST(RnnVad, SequenceBufferPushOps) {
  TestSequenceBufferPushOp<uint8_t, 32, 8>();   // Chunk size: 25%.
  TestSequenceBufferPushOp<uint8_t, 32, 16>();  // Chunk size: 50%.
  TestSequenceBufferPushOp<uint8_t, 32, 32>();  // Chunk size: 100%.
  TestSequenceBufferPushOp<uint8_t, 23, 7>();   // Non-integer ratio.
}

}  // namespace test
}  // namespace webrtc
