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
const ColorSpace::PrimaryID ColorSpace::kPrimaryIds[] = {
    ColorSpace::PrimaryID::kInvalid,     ColorSpace::PrimaryID::kBT709,
    ColorSpace::PrimaryID::kUNSPECIFIED, ColorSpace::PrimaryID::kBT470M,
    ColorSpace::PrimaryID::kBT470BG,     ColorSpace::PrimaryID::kSMPTE170M,
    ColorSpace::PrimaryID::kSMPTE240M,   ColorSpace::PrimaryID::kFILM,
    ColorSpace::PrimaryID::kBT2020,      ColorSpace::PrimaryID::kSMPTEST428,
    ColorSpace::PrimaryID::kSMPTEST431,  ColorSpace::PrimaryID::kSMPTEST432,
    ColorSpace::PrimaryID::kJEDECP22};

const ColorSpace::TransferID ColorSpace::kTransferIds[] = {
    ColorSpace::TransferID::kInvalid,
    ColorSpace::TransferID::kBT709,
    ColorSpace::TransferID::kUNSPECIFIED,
    ColorSpace::TransferID::kGAMMA22,
    ColorSpace::TransferID::kGAMMA28,
    ColorSpace::TransferID::kSMPTE170M,
    ColorSpace::TransferID::kSMPTE240M,
    ColorSpace::TransferID::kLINEAR,
    ColorSpace::TransferID::kLOG,
    ColorSpace::TransferID::kLOG_SQRT,
    ColorSpace::TransferID::kIEC61966_2_4,
    ColorSpace::TransferID::kBT1361_ECG,
    ColorSpace::TransferID::kIEC61966_2_1,
    ColorSpace::TransferID::kBT2020_10,
    ColorSpace::TransferID::kBT2020_12,
    ColorSpace::TransferID::kSMPTEST2084,
    ColorSpace::TransferID::kSMPTEST428,
    ColorSpace::TransferID::kARIB_STD_B67,
};

const ColorSpace::MatrixID ColorSpace::kMatrixIds[] = {
    ColorSpace::MatrixID::kRGB,         ColorSpace::MatrixID::kBT709,
    ColorSpace::MatrixID::kUNSPECIFIED, ColorSpace::MatrixID::kFCC,
    ColorSpace::MatrixID::kBT470BG,     ColorSpace::MatrixID::kSMPTE170M,
    ColorSpace::MatrixID::kSMPTE240M,   ColorSpace::MatrixID::kYCOCG,
    ColorSpace::MatrixID::kBT2020_NCL,  ColorSpace::MatrixID::kBT2020_CL,
    ColorSpace::MatrixID::kSMPTE2085,   ColorSpace::MatrixID::kCDNCLS,
    ColorSpace::MatrixID::kCDCLS,       ColorSpace::MatrixID::kBT2100_ICTCP,
    ColorSpace::MatrixID::kInvalid};

const ColorSpace::RangeID ColorSpace::kRangeIds[] = {
    ColorSpace::RangeID::kInvalid, ColorSpace::RangeID::kLimited,
    ColorSpace::RangeID::kFull, ColorSpace::RangeID::kDerived};

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

bool ColorSpace::set_primaries_from_uint8(uint8_t index) {
  return set_from_uint8(index, kPrimaryIds, &primaries_);
}

bool ColorSpace::set_transfer_from_uint8(uint8_t index) {
  return set_from_uint8(index, kTransferIds, &transfer_);
}

bool ColorSpace::set_matrix_from_uint8(uint8_t index) {
  return set_from_uint8(index, kMatrixIds, &matrix_);
}

bool ColorSpace::set_range_from_uint8(uint8_t index) {
  return set_from_uint8(index, kRangeIds, &range_);
}

void ColorSpace::set_hdr_metadata(const HdrMetadata* hdr_metadata) {
  hdr_metadata_ =
      hdr_metadata ? absl::make_optional(*hdr_metadata) : absl::nullopt;
}

}  // namespace webrtc
