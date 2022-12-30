/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/bit_writer.h"

namespace webrtc {

void BitWriter::WriteBits(uint64_t val, size_t bit_count) {
  RTC_DCHECK(valid_);
  const bool success = bit_writer_.WriteBits(val, bit_count);
  RTC_DCHECK(success);
}

void BitWriter::WriteExponentialGolomb(uint32_t val) {
  RTC_DCHECK(valid_);
  const bool success = bit_writer_.WriteExponentialGolomb(val);
  RTC_DCHECK(success);
}

void BitWriter::WriteBits(absl::string_view input) {
  RTC_DCHECK(valid_);
  for (char c : input) {
    WriteBits(static_cast<unsigned char>(c), CHAR_BIT);
  }
}

// Returns everything that was written so far.
// Nothing more may be written after this is called.
std::string BitWriter::GetString() {
  RTC_DCHECK(valid_);
  valid_ = false;

  size_t bytes;
  size_t bits;
  bit_writer_.GetCurrentOffset(&bytes, &bits);
  int bytes_required = bytes + (bits > 0 ? 1 : 0);
  buffer_.resize(bytes_required);

  std::string result;
  std::swap(buffer_, result);
  return result;
}

}  // namespace webrtc
