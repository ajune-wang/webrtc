/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packetizer_av1_unittest_helper.h"

#include <stdint.h>

#include <initializer_list>
#include <vector>

namespace webrtc {
namespace test {

Obu::Obu(uint8_t obu_type) : header_(obu_type | kObuSizePresentBit) {}

Obu& Obu::WithExtension(uint8_t extension) {
  extension_ = extension;
  header_ |= kObuExtensionPresentBit;
  return *this;
}
Obu& Obu::WithoutSize() {
  header_ &= ~kObuSizePresentBit;
  return *this;
}
Obu& Obu::WithPayload(std::vector<uint8_t> payload) {
  payload_ = std::move(payload);
  return *this;
}

std::vector<uint8_t> BuildAv1Frame(std::initializer_list<Obu> obus) {
  std::vector<uint8_t> raw;
  for (const Obu& obu : obus) {
    raw.push_back(obu.header_);
    if (obu.header_ & kObuExtensionPresentBit) {
      raw.push_back(obu.extension_);
    }
    if (obu.header_ & kObuSizePresentBit) {
      // write size in leb128 format.
      size_t payload_size = obu.payload_.size();
      while (payload_size >= 0x80) {
        raw.push_back(0x80 | (payload_size & 0x7F));
        payload_size >>= 7;
      }
      raw.push_back(payload_size);
    }
    raw.insert(raw.end(), obu.payload_.begin(), obu.payload_.end());
  }
  return raw;
}

}  // namespace test
}  // namespace webrtc
