/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/memory/bit_reader.h"

#include "absl/numeric/bits.h"
#include "rtc_base/checks.h"

namespace webrtc {

uint64_t BitReader::ReadBits(int bits) {
  RTC_DCHECK_GE(bits, 0);
  RTC_DCHECK_LE(bits, 64);
  if (remaining_bits_ < bits || bits == 0) {
    remaining_bits_ -= bits;
    return 0;
  }

  int remaining_bits_in_first_byte = remaining_bits_ % 8;
  remaining_bits_ -= bits;
  if (remaining_bits_in_first_byte > bits) {
    // Reading fewer bits than what's left in the current byte, just
    // return the portion of this byte that we need.
    int offset = (remaining_bits_in_first_byte - bits);
    return ((*bytes_) >> offset) & ((1 << bits) - 1);
  }

  uint64_t result = 0;
  if (remaining_bits_in_first_byte > 0) {
    result = (*bytes_++) & ((1 << remaining_bits_in_first_byte) - 1);
    bits -= remaining_bits_in_first_byte;
  }

  // Read as many full bytes as we cans.
  while (bits >= 8) {
    result = (result << 8) | *bytes_++;
    bits -= 8;
  }
  // Whatever is left to read is smaller than a byte, so grab just the needed
  // bits and shift them into the lowest bits.
  if (bits > 0) {
    result <<= bits;
    result |= (*bytes_ >> (8 - bits));
  }
  return result;
}

int BitReader::ReadBit() {
  --remaining_bits_;
  if (remaining_bits_ < 0) {
    return 0;
  }

  int bit_position = remaining_bits_ % 8;
  if (bit_position == 0) {
    // Read the last bit from current byte and move to the next byte.
    return (*bytes_++) & 0x01;
  }

  return (*bytes_ >> bit_position) & 0x01;
}

void BitReader::ConsumeBits(int bits) {
  RTC_DCHECK_GE(bits, 0);
  int remaining_bytes = (remaining_bits_ + 7) / 8;
  remaining_bits_ -= bits;
  int new_remaining_bytes = (remaining_bits_ + 7) / 8;
  // When `new_remaining_bytes` is negative, this byte adjustement might be
  // incorrect, but that doesn't matter because in such case BitReader in error
  // state and will not longer derefernce `bytes_`.
  bytes_ += (remaining_bytes - new_remaining_bytes);
}

uint32_t BitReader::ReadNonSymmetric(uint32_t num_values) {
  RTC_DCHECK_GT(num_values, 0);
  RTC_DCHECK_LE(num_values, uint32_t{1} << 31);

  int count_bits = absl::bit_width(num_values);
  uint32_t num_min_bits_values = (uint32_t{1} << count_bits) - num_values;

  uint64_t val = ReadBits(count_bits - 1);
  if (val < num_min_bits_values) {
    return val;
  }
  return (val << 1) + ReadBit() - num_min_bits_values;
}

uint32_t BitReader::ReadExponentialGolomb() {
  // Count the number of leading 0.
  int zero_bit_count = 0;
  while (ReadBit() == 0) {
    if (!Ok())
      return 0;
    ++zero_bit_count;
  }
  if (zero_bit_count >= 32) {
    // Golob value won't fit into 32 bits of the return value. Fail the parse.
    Invalidate();
    return 0;
  }

  // The bit count of the value is the number of zeros + 1.
  // However the first '1' was already read above.
  return (uint32_t{1} << zero_bit_count) + ReadBits(zero_bit_count) - 1;
}

int32_t BitReader::ReadSignedExponentialGolomb() {
  uint32_t unsigned_val = ReadExponentialGolomb();
  if ((unsigned_val & 1) == 0) {
    return -static_cast<int32_t>(unsigned_val / 2);
  } else {
    return (unsigned_val + 1) / 2;
  }
}

}  // namespace webrtc
