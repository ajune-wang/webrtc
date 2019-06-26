/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/non_symmetric_bit_helper.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

int FloorLog2(uint32_t num_values) {
  RTC_DCHECK_GT(num_values, 0);
  int s = 0;
  while (num_values != 0) {
    num_values >>= 1;
    s++;
  }
  return s - 1;
}

}  // namespace

NonSymmetricBitHelper::NonSymmetricBitHelper(uint32_t num_values)
    : min_bits_(FloorLog2(num_values)),
      num_min_bits_values_((1 << (min_bits_ + 1)) - num_values) {}

bool NonSymmetricBitHelper::Read(rtc::BitBuffer* buffer,
                                 uint32_t* value) const {
  if (!buffer->ReadBits(value, min_bits_))
    return false;
  if (*value < num_min_bits_values_)
    return true;
  uint32_t extra_bit;
  if (!buffer->ReadBits(&extra_bit, /*bit_count=*/1))
    return false;
  *value = (*value << 1) + extra_bit - num_min_bits_values_;
  return true;
}

bool NonSymmetricBitHelper::Write(rtc::BitBufferWriter* buffer,
                                  uint32_t value) const {
  // Check value < num_values
  RTC_DCHECK_GT(1 << (min_bits_ + 1), value + num_min_bits_values_);
  if (value < num_min_bits_values_)
    return buffer->WriteBits(value, min_bits_);
  return buffer->WriteBits(value + num_min_bits_values_, min_bits_ + 1);
}

}  // namespace webrtc
