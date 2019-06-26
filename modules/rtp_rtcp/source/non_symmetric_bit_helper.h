/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_NON_SYMMETRIC_BIT_HELPER_H_
#define MODULES_RTP_RTCP_SOURCE_NON_SYMMETRIC_BIT_HELPER_H_

#include <stdint.h>

#include "rtc_base/bit_buffer.h"

namespace webrtc {

// An extension to BitBuffer/BitBufferWriter for storing unsigned integer
// with known maximum value.
// Reads/writes values in range [0, num_values-1] inclusive.
// if num_values is n'th power of two, the helper uses n bits for all values.
// Otherwise smaller values are stored using 1 less bit than larger values.
class NonSymmetricBitHelper {
 public:
  explicit NonSymmetricBitHelper(uint32_t num_values);

  // Returns number of bits needed to read/write the |value|.
  int BitSize(uint32_t value) const {
    return (value < num_min_bits_values_) ? min_bits_ : (min_bits_ + 1);
  }

  bool Read(rtc::BitBuffer* buffer, uint32_t* value) const;
  bool Write(rtc::BitBufferWriter* buffer, uint32_t value) const;

 private:
  int min_bits_;
  uint32_t num_min_bits_values_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_NON_SYMMETRIC_BIT_HELPER_H_
