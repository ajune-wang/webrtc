/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MEMORY_BIT_READER_H_
#define RTC_BASE_MEMORY_BIT_READER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/strings/string_view.h"
#include "api/array_view.h"

namespace webrtc {

// A class to parse bitstream, optimized for successful parsing and binary size.
// Individual calls to `Read` and `ConsumeBits` never fail. Instead user of this
// class should verify if parsing was successful by calling `Ok`.
// That can be done once after multiple reads.
// Byte order is assumed big-endian/network.
class BitReader {
 public:
  explicit BitReader(rtc::ArrayView<const uint8_t> bytes);
  explicit BitReader(absl::string_view bytes);
  BitReader(const BitReader&) = default;
  BitReader& operator=(const BitReader&) = default;
  ~BitReader() = default;

  // Return number of unread bits in the buffer, or negative number if there
  // was a reading error.
  int RemainingBitCount() const { return remaining_bits_; }

  // Returns `true` iff all calls to `Read` and `ConsumeBits` were successful.
  bool Ok() const { return remaining_bits_ >= 0; }
  void Invalidate() { remaining_bits_ = -1; }

  // Moves current position `bits` bits forward.
  void ConsumeBits(int bits);

  // Reads single bit. Returns 0 or 1.
  ABSL_MUST_USE_RESULT int ReadBit();

  // Reads `bits` from the bitstream and returns result as unsigned integer.
  ABSL_MUST_USE_RESULT uint64_t ReadBits(int bits);

  template <typename T,
            typename std::enable_if<std::is_unsigned<T>::value &&
                                    sizeof(T) <= 8>::type* = nullptr>
  T Read() {
    return static_cast<T>(ReadBits(sizeof(T) * 8));
  }

  template <>
  bool Read<bool>() {
    return ReadBit() != 0;
  }

  // Reads value in range [0, num_values - 1].
  // This encoding is similar to ReadBits(val, Ceil(Log2(num_values)),
  // but reduces wastage incurred when encoding non-power of two value ranges
  // Non symmetric values are encoded as:
  // 1) n = countbits(num_values)
  // 2) k = (1 << n) - num_values
  // Value v in range [0, k - 1] is encoded in (n-1) bits.
  // Value v in range [k, num_values - 1] is encoded as (v+k) in n bits.
  // https://aomediacodec.github.io/av1-spec/#nsn
  uint32_t ReadNonSymmetric(uint32_t num_values);

  // Reads the exponential golomb encoded value at the current offset.
  // Exponential golomb values are encoded as:
  // 1) x = source val + 1
  // 2) In binary, write [bit_width(x) - 1] 0s, then x
  // To decode, we count the number of leading 0 bits, read that many + 1 bits,
  // and increment the result by 1.
  // Fails the parsing if the value wouldn't fit in a uint32_t.
  uint32_t ReadExponentialGolomb();

  // Reads signed exponential golomb values at the current offset. Signed
  // exponential golomb values are just the unsigned values mapped to the
  // sequence 0, 1, -1, 2, -2, etc. in order.
  int32_t ReadSignedExponentialGolomb();

 private:
  // Next byte with at least one unread bit.
  const uint8_t* bytes_;
  // Number of bit remained to read.
  int remaining_bits_;
};

inline BitReader::BitReader(rtc::ArrayView<const uint8_t> bytes)
    : bytes_(bytes.data()), remaining_bits_(bytes.size() * 8) {}

inline BitReader::BitReader(absl::string_view bytes)
    : bytes_(reinterpret_cast<const uint8_t*>(bytes.data())),
      remaining_bits_(bytes.size() * 8) {}

}  // namespace webrtc

#endif  // RTC_BASE_MEMORY_BIT_READER_H_
