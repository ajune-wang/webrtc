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
namespace {

// Returns the lowest (right-most) `bit_count` bits in `byte`.
uint8_t LowestBits(uint8_t byte, size_t bit_count) {
  RTC_DCHECK_LE(bit_count, 8);
  return byte & ((1 << bit_count) - 1);
}

// Returns the highest (left-most) `bit_count` bits in `byte`, shifted to the
// lowest bits (to the right).
uint8_t HighestBits(uint8_t byte, size_t bit_count) {
  RTC_DCHECK_LE(bit_count, 8);
  uint8_t shift = 8 - static_cast<uint8_t>(bit_count);
  uint8_t mask = 0xFF << shift;
  return (byte & mask) >> shift;
}

}  // namespace

uint64_t BitReader::Read(int bits) {
  RTC_DCHECK_GE(bits, 0);
  RTC_DCHECK_LE(bits, 64);
  if (remaining_bits_ < bits || bits == 0) {
    remaining_bits_ -= bits;
    return 0;
  }

  int remaining_bits_in_first_byte = remaining_bits_ % 8;
  if (remaining_bits_in_first_byte > bits) {
    // Reading fewer bits than what's left in the current byte, just
    // return the portion of this byte that we need.
    int offset = (remaining_bits_in_first_byte - bits);
    remaining_bits_ -= bits;
    return ((*bytes_) >> offset) & ((1 << bits) - 1);
  }

  uint64_t result = 0;
  if (remaining_bits_in_first_byte > 0) {
    result = LowestBits(*bytes_++, remaining_bits_in_first_byte);
    remaining_bits_ -= remaining_bits_in_first_byte;
    bits -= remaining_bits_in_first_byte;
  }

  // Read as many full bytes as we cans.
  while (bits >= 8) {
    result = (result << 8) | *bytes_++;
    remaining_bits_ -= 8;
    bits -= 8;
  }
  // Whatever is left to read is smaller than a byte, so grab just the needed
  // bits and shift them into the lowest bits.
  if (bits > 0) {
    result <<= bits;
    result |= HighestBits(*bytes_, bits);
    remaining_bits_ -= bits;
  }
  return result;
}

void BitReader::Consume(int bits) {
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

  uint64_t val = Read(count_bits - 1);
  if (val < num_min_bits_values) {
    return val;
  }
  return (val << 1) + Read(/*bits=*/1) - num_min_bits_values;
}

uint32_t BitReader::ReadExponentialGolomb() {
  // Count the number of leading 0.
  size_t zero_bit_count = 0;
  while (Ok() && Read(/*bits=*/1) == 0) {
    zero_bit_count++;
  }
  if (zero_bit_count >= 32) {
    // Golob value is too large. Fail the parse.
    remaining_bits_ = -1;
    return 0;
  }

  // The bit count of the value is the number of zeros + 1.
  // However the first '1' was already read above.
  return (uint32_t{1} << zero_bit_count) + Read(zero_bit_count) - 1;
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
