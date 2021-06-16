/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_AV1_UNITTEST_HELPER_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_AV1_UNITTEST_HELPER_H_

#include <stdint.h>

#include <initializer_list>
#include <utility>
#include <vector>

namespace webrtc {
namespace test {

constexpr uint8_t kNewCodedVideoSequenceBit = 0b00'00'1000;
// All obu types offset by 3 to take correct position in the obu_header.
constexpr uint8_t kObuTypeSequenceHeader = 1 << 3;
constexpr uint8_t kObuTypeTemporalDelimiter = 2 << 3;
constexpr uint8_t kObuTypeFrameHeader = 3 << 3;
constexpr uint8_t kObuTypeTileGroup = 4 << 3;
constexpr uint8_t kObuTypeMetadata = 5 << 3;
constexpr uint8_t kObuTypeFrame = 6 << 3;
constexpr uint8_t kObuTypeTileList = 8 << 3;
constexpr uint8_t kObuExtensionPresentBit = 0b0'0000'100;
constexpr uint8_t kObuSizePresentBit = 0b0'0000'010;
constexpr uint8_t kObuExtensionS1T1 = 0b001'01'000;

class Obu {
 public:
  explicit Obu(uint8_t obu_type);

  Obu& WithExtension(uint8_t extension);
  Obu& WithoutSize();
  Obu& WithPayload(std::vector<uint8_t> payload);

 private:
  friend std::vector<uint8_t> BuildAv1Frame(std::initializer_list<Obu> obus);
  uint8_t header_;
  uint8_t extension_ = 0;
  std::vector<uint8_t> payload_;
};

std::vector<uint8_t> BuildAv1Frame(std::initializer_list<Obu> obus);

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_AV1_UNITTEST_HELPER_H_
