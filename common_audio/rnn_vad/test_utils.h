/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_TEST_UTILS_H_
#define COMMON_AUDIO_RNN_VAD_TEST_UTILS_H_

#include <algorithm>
#include <array>
#include <cfenv>
#include <fstream>
#include <string>

#include "api/array_view.h"

namespace webrtc {
namespace test {

constexpr float kExpectNearTolerance = 3e-6f;

// Element-wise comparison for rtc::ArrayView<float> pairs.
void ExpectNear(rtc::ArrayView<const float> a,
                rtc::ArrayView<const float> b,
                const float tolerance = kExpectNearTolerance);

// Reader for binary files consisting of an arbitrary long sequence of elements
// having type T. It is possible to read and cast to another type D at once.
// ReadChunk() reads chunks of size N.
template <typename T, size_t N = 1, typename D = T>
class BinaryFileReader {
 public:
  explicit BinaryFileReader(const std::string& file_path)
      : is_(file_path, std::ios::binary | std::ios::ate),
        data_length_(is_.tellg() / sizeof(T)) {
    RTC_CHECK(is_);
    is_.seekg(0, is_.beg);
  }
  ~BinaryFileReader() = default;
  size_t data_length() const { return data_length_; }
  bool ReadValue(D* dst) {
    if (std::is_same<T, D>::value) {
      is_.read(reinterpret_cast<char*>(dst), sizeof(T));
    } else {
      T v;
      is_.read(reinterpret_cast<char*>(&v), sizeof(T));
      *dst = static_cast<D>(v);
    }
    return is_.gcount() == sizeof(T);
  }
  bool ReadChunk(rtc::ArrayView<D, N> dst) {
    constexpr std::streamsize bytes_to_read = N * sizeof(T);
    if (std::is_same<T, D>::value) {
      is_.read(reinterpret_cast<char*>(dst.data()), bytes_to_read);
    } else {
      std::array<T, N> buf;
      is_.read(reinterpret_cast<char*>(buf.data()), bytes_to_read);
      std::transform(buf.begin(), buf.end(), dst.begin(),
                     [](const T& v) -> D { return static_cast<D>(v); });
    }
    return is_.gcount() == bytes_to_read;
  }

 private:
  std::ifstream is_;
  const size_t data_length_;
};

}  // namespace test
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_TEST_UTILS_H_
