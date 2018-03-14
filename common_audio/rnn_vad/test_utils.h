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
#include <cfenv>
#include <fstream>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

// Initial state for the HP filter in the unit tests.
constexpr float kHighPassFilterStatus[2] = {-45.993244171142578125f,
                                            45.930263519287109375f};

// Parameters for bit exactness tests.
constexpr size_t kSampleRate48k = 48000;
constexpr size_t k48k10msFrameSize = 480;
constexpr size_t k48k20msFrameSize = 960;
constexpr size_t k48k20msFftSize = 1024;
constexpr size_t k48k20msNumFftCoeffs = k48k20msFftSize / 2 + 1;

constexpr float kExpectNearTolerance = 3e-6f;

// Element-wise comparison for rtc::ArrayView<float> pairs.
void ExpectNear(rtc::ArrayView<const float> a,
                rtc::ArrayView<const float> b,
                const float tolerance = kExpectNearTolerance);

// Reader for binary files consisting of an arbitrary long sequence of elements
// having type T. It is possible to read and cast to another type D at once.
template <typename T, typename D = T>
class BinaryFileReader {
 public:
  explicit BinaryFileReader(const std::string& file_path,
                            const size_t chunk_size = 1)
      : is_(file_path, std::ios::binary | std::ios::ate),
        data_length_(is_.tellg() / sizeof(T)),
        chunk_size_(chunk_size) {
    RTC_CHECK_LT(0, chunk_size_);
    RTC_CHECK(is_);
    is_.seekg(0, is_.beg);
    buf_.resize(chunk_size_);
  }
  // Disable copy (and move) semantics.
  BinaryFileReader(const BinaryFileReader&) = delete;
  BinaryFileReader& operator=(const BinaryFileReader&) = delete;
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
  bool ReadChunk(rtc::ArrayView<D> dst) {
    RTC_DCHECK_EQ(chunk_size_, dst.size());
    const std::streamsize bytes_to_read = chunk_size_ * sizeof(T);
    if (std::is_same<T, D>::value) {
      is_.read(reinterpret_cast<char*>(dst.data()), bytes_to_read);
    } else {
      is_.read(reinterpret_cast<char*>(buf_.data()), bytes_to_read);
      std::transform(buf_.begin(), buf_.end(), dst.begin(),
                     [](const T& v) -> D { return static_cast<D>(v); });
    }
    return is_.gcount() == bytes_to_read;
  }
  void SeekForward(size_t items) { is_.seekg(items * sizeof(T), is_.cur); }

 private:
  std::ifstream is_;
  const size_t data_length_;
  const size_t chunk_size_;
  std::vector<T> buf_;
};

}  // namespace test
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_TEST_UTILS_H_
