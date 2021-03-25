/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_SCTP_DCSCTP_PACKET_BOUNDED_BYTE_WRITER_H_
#define MEDIA_SCTP_DCSCTP_PACKET_BOUNDED_BYTE_WRITER_H_

#include <algorithm>

#include "absl/base/internal/endian.h"
#include "api/array_view.h"

namespace dcsctp {

// BoundedByteWriter wraps an ArrayView and divides it into two parts; A fixed
// size - which is the template parameter - and a variable size, which is what
// remains in `data` after the `FixedSize`.
//
// The BoundedByteWriter provides methods to write big endian numbers to the
// FixedSize portion of the buffer, and these are written with static bounds
// checking, to avoid out-of-bounds accesses without a run-time penalty.
//
// The variable sized portion can either be used to create sub-writers, which
// themselves would provide compile-time bounds-checking, or data can be copied
// to it.
template <int FixedSize>
class BoundedByteWriter {
 public:
  explicit BoundedByteWriter(rtc::ArrayView<uint8_t> data) : data_(data) {
    RTC_DCHECK(data.size() >= FixedSize);
  }

  template <size_t offset>
  void Store64(uint64_t value) {
    static_assert(offset + sizeof(uint64_t) <= FixedSize, "Out-of-bounds");
    static_assert((offset % sizeof(uint32_t)) == 0, "Invalid alignment");
    absl::big_endian::Store64(&data_[offset], value);
  }

  template <size_t offset>
  void Store32(uint32_t value) {
    static_assert(offset + sizeof(uint32_t) <= FixedSize, "Out-of-bounds");
    static_assert((offset % sizeof(uint32_t)) == 0, "Invalid alignment");
    absl::big_endian::Store32(&data_[offset], value);
  }

  template <size_t offset>
  void Store16(uint16_t value) {
    static_assert(offset + sizeof(uint16_t) <= FixedSize, "Out-of-bounds");
    static_assert((offset % sizeof(uint16_t)) == 0, "Invalid alignment");
    absl::big_endian::Store16(&data_[offset], value);
  }

  template <size_t offset>
  void Store8(uint8_t value) {
    static_assert(offset + sizeof(uint8_t) <= FixedSize, "Out-of-bounds");
    data_[offset] = value;
  }

  template <size_t SubSize>
  BoundedByteWriter<SubSize> sub_writer(size_t variable_offset) {
    RTC_DCHECK(FixedSize + variable_offset + SubSize <= data_.size());

    return BoundedByteWriter<SubSize>(
        data_.subview(FixedSize + variable_offset, SubSize));
  }

  void CopyToVariableData(rtc::ArrayView<const uint8_t> source) {
    memcpy(data_.data() + FixedSize, source.data(),
           std::min(source.size(), data_.size() - FixedSize));
  }

 private:
  rtc::ArrayView<uint8_t> data_;
};
}  // namespace dcsctp

#endif  // MEDIA_SCTP_DCSCTP_PACKET_BOUNDED_BYTE_WRITER_H_
