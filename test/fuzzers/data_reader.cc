/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/data_reader.h"

#include <algorithm>
#include <cstring>

namespace webrtc {

DataReader::DataReader(const uint8_t* data, size_t data_size)
    : data_(data), data_size_(data_size) {}

void DataReader::CopyTo(void* destination, size_t dest_size) {
  memset(destination, 0, dest_size);

  size_t bytes_to_copy = std::min(data_size_ - offset_, dest_size);
  memcpy(destination, data_ + offset_, bytes_to_copy);
  offset_ += bytes_to_copy;
}

bool DataReader::MoreToRead() const {
  return offset_ < data_size_;
}

}  // namespace webrtc
