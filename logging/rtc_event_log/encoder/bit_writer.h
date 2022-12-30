/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_ENCODER_BIT_WRITER_H_
#define LOGGING_RTC_EVENT_LOG_ENCODER_BIT_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

// Wrap BitBufferWriter and extend its functionality by (1) keeping track of
// the number of bits written and (2) owning its buffer.
class BitWriter final {
 public:
  explicit BitWriter(size_t byte_count)
      : buffer_(byte_count, '\0'),
        bit_writer_(reinterpret_cast<uint8_t*>(&buffer_[0]), buffer_.size()),
        valid_(true) {
    RTC_DCHECK_GT(byte_count, 0);
  }

  BitWriter(const BitWriter&) = delete;
  BitWriter& operator=(const BitWriter&) = delete;

  void WriteBits(uint64_t val, size_t bit_count);

  void WriteExponentialGolomb(uint32_t val);

  void WriteBits(absl::string_view input);

  // Returns everything that was written so far.
  // Nothing more may be written after this is called.
  std::string GetString();

 private:
  std::string buffer_;
  rtc::BitBufferWriter bit_writer_;
  bool valid_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_ENCODER_BIT_WRITER_H_
