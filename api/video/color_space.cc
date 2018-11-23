/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/color_space.h"

#include "rtc_base/checks.h"

namespace webrtc {

ColorSpace::ColorSpace() = default;
ColorSpace::ColorSpace(const ColorSpace& other) = default;
ColorSpace::ColorSpace(ColorSpace&& other) = default;
ColorSpace& ColorSpace::operator=(const ColorSpace& other) = default;

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range)
    : ColorSpace(primaries, transfer, matrix, range, nullptr) {}

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range,
                       const HdrMetadata* hdr_metadata)
    : primaries_(primaries),
      transfer_(transfer),
      matrix_(matrix),
      range_(range),
      hdr_metadata_(hdr_metadata ? absl::make_optional(*hdr_metadata)
                                 : absl::nullopt) {}

ColorSpace::PrimaryID ColorSpace::primaries() const {
  return primaries_;
}

ColorSpace::TransferID ColorSpace::transfer() const {
  return transfer_;
}

ColorSpace::MatrixID ColorSpace::matrix() const {
  return matrix_;
}

ColorSpace::RangeID ColorSpace::range() const {
  return range_;
}

const HdrMetadata* ColorSpace::hdr_metadata() const {
  return hdr_metadata_ ? &*hdr_metadata_ : nullptr;
}

bool ColorSpace::set_primaries_from_uint8(uint8_t enum_value) {
  static const PrimaryID kPrimaryIds[] = {
      PrimaryID::kInvalid,    PrimaryID::kBT709,      PrimaryID::kUNSPECIFIED,
      PrimaryID::kBT470M,     PrimaryID::kBT470BG,    PrimaryID::kSMPTE170M,
      PrimaryID::kSMPTE240M,  PrimaryID::kFILM,       PrimaryID::kBT2020,
      PrimaryID::kSMPTEST428, PrimaryID::kSMPTEST431, PrimaryID::kSMPTEST432,
      PrimaryID::kJEDECP22};
  static const uint64_t enum_bitmask = create_enum_bitmask(kPrimaryIds);

  return set_from_uint8(enum_value, enum_bitmask, &primaries_);
}

bool ColorSpace::set_transfer_from_uint8(uint8_t enum_value) {
  static const TransferID kTransferIds[] = {
      TransferID::kInvalid,      TransferID::kBT709,
      TransferID::kUNSPECIFIED,  TransferID::kGAMMA22,
      TransferID::kGAMMA28,      TransferID::kSMPTE170M,
      TransferID::kSMPTE240M,    TransferID::kLINEAR,
      TransferID::kLOG,          TransferID::kLOG_SQRT,
      TransferID::kIEC61966_2_4, TransferID::kBT1361_ECG,
      TransferID::kIEC61966_2_1, TransferID::kBT2020_10,
      TransferID::kBT2020_12,    TransferID::kSMPTEST2084,
      TransferID::kSMPTEST428,   TransferID::kARIB_STD_B67};
  static const uint64_t enum_bitmask = create_enum_bitmask(kTransferIds);

  return set_from_uint8(enum_value, enum_bitmask, &transfer_);
}

bool ColorSpace::set_matrix_from_uint8(uint8_t enum_value) {
  static const MatrixID kMatrixIds[] = {
      MatrixID::kRGB,       MatrixID::kBT709,        MatrixID::kUNSPECIFIED,
      MatrixID::kFCC,       MatrixID::kBT470BG,      MatrixID::kSMPTE170M,
      MatrixID::kSMPTE240M, MatrixID::kYCOCG,        MatrixID::kBT2020_NCL,
      MatrixID::kBT2020_CL, MatrixID::kSMPTE2085,    MatrixID::kCDNCLS,
      MatrixID::kCDCLS,     MatrixID::kBT2100_ICTCP, MatrixID::kInvalid};
  static const uint64_t enum_bitmask = create_enum_bitmask(kMatrixIds);

  return set_from_uint8(enum_value, enum_bitmask, &matrix_);
}

bool ColorSpace::set_range_from_uint8(uint8_t enum_value) {
  static const RangeID kRangeIds[] = {RangeID::kInvalid, RangeID::kLimited,
                                      RangeID::kFull, RangeID::kDerived};
  static const uint64_t enum_bitmask = create_enum_bitmask(kRangeIds);

  return set_from_uint8(enum_value, enum_bitmask, &range_);
}

void ColorSpace::set_hdr_metadata(const HdrMetadata* hdr_metadata) {
  hdr_metadata_ =
      hdr_metadata ? absl::make_optional(*hdr_metadata) : absl::nullopt;
}

template <typename T>
bool ColorSpace::set_from_uint8(uint8_t enum_value,
                                uint64_t enum_bitmask,
                                T* out) {
  if ((enum_value < 64) & (enum_bitmask >> enum_value)) {
    *out = static_cast<T>(enum_value);
    return true;
  }
  return false;
}

template <typename T, size_t N>
uint64_t ColorSpace::create_enum_bitmask(const T (&ids)[N]) {
  // Create a bitmask where each bit corresponds to one potential enum value.
  // The bit is set to one if the corresponding enum exists. Only works for
  // enums with values less than 64.
  uint64_t enum_bitmask = 0;
  for (T id : ids) {
    RTC_DCHECK_LT(static_cast<uint8_t>(id), 64);
    enum_bitmask |= (uint64_t{1} << static_cast<uint8_t>(id));
  }
  return enum_bitmask;
}

}  // namespace webrtc
