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
  const PrimaryID kPrimaryIds[] = {
      PrimaryID::kInvalid,    PrimaryID::kBT709,      PrimaryID::kUNSPECIFIED,
      PrimaryID::kBT470M,     PrimaryID::kBT470BG,    PrimaryID::kSMPTE170M,
      PrimaryID::kSMPTE240M,  PrimaryID::kFILM,       PrimaryID::kBT2020,
      PrimaryID::kSMPTEST428, PrimaryID::kSMPTEST431, PrimaryID::kSMPTEST432,
      PrimaryID::kJEDECP22};

  return set_from_uint8(enum_value, kPrimaryIds, &primaries_);
}

bool ColorSpace::set_transfer_from_uint8(uint8_t enum_value) {
  const TransferID kTransferIds[] = {
      TransferID::kInvalid,      TransferID::kBT709,
      TransferID::kUNSPECIFIED,  TransferID::kGAMMA22,
      TransferID::kGAMMA28,      TransferID::kSMPTE170M,
      TransferID::kSMPTE240M,    TransferID::kLINEAR,
      TransferID::kLOG,          TransferID::kLOG_SQRT,
      TransferID::kIEC61966_2_4, TransferID::kBT1361_ECG,
      TransferID::kIEC61966_2_1, TransferID::kBT2020_10,
      TransferID::kBT2020_12,    TransferID::kSMPTEST2084,
      TransferID::kSMPTEST428,   TransferID::kARIB_STD_B67};

  return set_from_uint8(enum_value, kTransferIds, &transfer_);
}

bool ColorSpace::set_matrix_from_uint8(uint8_t enum_value) {
  const MatrixID kMatrixIds[] = {
      MatrixID::kRGB,       MatrixID::kBT709,        MatrixID::kUNSPECIFIED,
      MatrixID::kFCC,       MatrixID::kBT470BG,      MatrixID::kSMPTE170M,
      MatrixID::kSMPTE240M, MatrixID::kYCOCG,        MatrixID::kBT2020_NCL,
      MatrixID::kBT2020_CL, MatrixID::kSMPTE2085,    MatrixID::kCDNCLS,
      MatrixID::kCDCLS,     MatrixID::kBT2100_ICTCP, MatrixID::kInvalid};

  return set_from_uint8(enum_value, kMatrixIds, &matrix_);
}

bool ColorSpace::set_range_from_uint8(uint8_t enum_value) {
  const RangeID kRangeIds[] = {RangeID::kInvalid, RangeID::kLimited,
                               RangeID::kFull, RangeID::kDerived};

  return set_from_uint8(enum_value, kRangeIds, &range_);
}

void ColorSpace::set_hdr_metadata(const HdrMetadata* hdr_metadata) {
  hdr_metadata_ =
      hdr_metadata ? absl::make_optional(*hdr_metadata) : absl::nullopt;
}

template <typename T, size_t N>
bool ColorSpace::set_from_uint8(uint8_t enum_value, const T (&ids)[N], T* out) {
  for (T id : ids) {
    if (enum_value == static_cast<uint8_t>(id)) {
      *out = id;
      return true;
    }
  }
  return false;
}

}  // namespace webrtc
