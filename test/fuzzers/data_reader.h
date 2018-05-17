/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FUZZERS_DATA_READER_H_
#define TEST_FUZZERS_DATA_READER_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace webrtc {
// When DataReader runs out of data provided in the constructor it will
// just set/return 0 instead.
class DataReader {
 public:
  DataReader(const uint8_t* data, size_t data_size);

  // Return false when all |data_| has been consumed.
  bool MoreToRead() const;

  void CopyTo(void* destination, size_t dest_size);

  template <typename T>
  void CopyTo(T* object) {
    CopyTo(object, sizeof(typename std::remove_pointer<T>::type));
  }

 private:
  const uint8_t* const data_;
  size_t data_size_;
  size_t offset_ = 0;
};
}  // namespace webrtc

#endif  // TEST_FUZZERS_DATA_READER_H_
