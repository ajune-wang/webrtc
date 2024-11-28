/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_annexb_bitstream_builder.h"

namespace webrtc {

H265AnnexBBitstreamBuilder::H265AnnexBBitstreamBuilder(
    bool insert_emulation_prevention_bytes)
    : insert_emulation_prevention_bytes_(insert_emulation_prevention_bytes) {
  Reset();
}

H265AnnexBBitstreamBuilder::~H265AnnexBBitstreamBuilder() = default;

void H265AnnexBBitstreamBuilder::Reset() {
  pos_ = 0;
  bits_in_buffer_ = 0;
  reg_ = 0;

  data_.Clear();

  if (data_.capacity() == 0) {
    Grow();
  }

  bits_left_in_reg_ = kRegBitSize;
  in_nalu_ = false;
}

void H265AnnexBBitstreamBuilder::Grow() {
  data_.EnsureCapacity(data_.capacity() + kGrowBytes);
}

void H265AnnexBBitstreamBuilder::FlushReg() {
  size_t bits_in_reg = kRegBitSize - bits_left_in_reg_;
  if (bits_in_reg == 0u) {
    return;
  }

  // Align up to the nearest byte
  size_t bytes_in_reg = (bits_in_reg + 7) / 8;
  reg_ <<= (kRegBitSize - bits_in_reg);

  // Convert to MSB and append as such to the stream.
  std::array<uint8_t, 8> reg_be;
  for (size_t i = 0; i < 8; ++i) {
    reg_be[7 - i] = static_cast<uint8_t>(reg_ >> (i * 8));
  }

  if (insert_emulation_prevention_bytes_ && in_nalu_) {
    constexpr uint8_t kEmulationByte = 0x03u;

    for (size_t i = 0; i < bytes_in_reg; ++i) {
      if (data_.size() >= 2u && data_[pos_ - 2u] == 0 &&
          data_[pos_ - 1u] == 0u && reg_be[i] <= kEmulationByte) {
        if (pos_ + 1u > data_.capacity()) {
          Grow();
        }
        data_.AppendData(&kEmulationByte, 1);
        pos_++;
        bits_in_buffer_ += 8u;
      }
      if (pos_ + 1u > data_.capacity()) {
        Grow();
      }
      data_.AppendData(&reg_be[i], 1);
      pos_++;
      bits_in_buffer_ += 8u;
    }
  } else {
    if (pos_ + bytes_in_reg > data_.capacity()) {
      Grow();
    }
    bits_in_buffer_ = pos_ * 8u + bits_in_reg;
    for (size_t i = 0; i < bytes_in_reg; ++i) {
      data_.AppendData(&reg_be[i], 1);
      pos_++;
    }
  }

  reg_ = 0u;
  bits_left_in_reg_ = kRegBitSize;
}

void H265AnnexBBitstreamBuilder::AppendU64(size_t num_bits, uint64_t val) {
  RTC_CHECK_LE(num_bits, kRegBitSize);

  while (num_bits > 0u) {
    if (bits_left_in_reg_ == 0u) {
      FlushReg();
    }

    uint64_t bits_to_write =
        num_bits > bits_left_in_reg_ ? bits_left_in_reg_ : num_bits;
    uint64_t val_to_write = (val >> (num_bits - bits_to_write));
    if (bits_to_write < 64u) {
      val_to_write &= ((1ull << bits_to_write) - 1);
      reg_ <<= bits_to_write;
      reg_ |= val_to_write;
    } else {
      reg_ = val_to_write;
    }
    num_bits -= bits_to_write;
    bits_left_in_reg_ -= bits_to_write;
  }
}

void H265AnnexBBitstreamBuilder::AppendBool(bool val) {
  if (bits_left_in_reg_ == 0u) {
    FlushReg();
  }

  reg_ <<= 1;
  reg_ |= (static_cast<uint64_t>(val) & 1u);
  --bits_left_in_reg_;
}

void H265AnnexBBitstreamBuilder::AppendSE(int val) {
  if (val > 0) {
    AppendUE(val * 2 - 1);
  } else {
    AppendUE(-val * 2);
  }
}

void H265AnnexBBitstreamBuilder::AppendUE(unsigned int val) {
  size_t num_zeros = 0u;
  unsigned int v = val + 1u;

  while (v > 1) {
    v >>= 1;
    ++num_zeros;
  }

  AppendBits(num_zeros, 0);
  AppendBits(num_zeros + 1, val + 1u);
}

#define DCHECK_FINISHED()                       \
  RTC_DCHECK_EQ(bits_left_in_reg_, kRegBitSize) \
      << "Pending bits not yet written "        \
         "to the buffer, call "                 \
         "FinishNALU() first."

void H265AnnexBBitstreamBuilder::BeginNALU(H265::NaluType nalu_type,
                                           uint8_t spatial_id,
                                           uint8_t temporal_id) {
  RTC_DCHECK(!in_nalu_);
  DCHECK_FINISHED();

  RTC_DCHECK_LE(nalu_type, H265::NaluType::kSuffixSei);

  AppendBits(32, 0x00000001);
  Flush();
  in_nalu_ = true;
  AppendBits(1, 0);                // forbidden_zero_bit
  AppendBits(6, nalu_type);        // nal_unit_type
  AppendBits(6, spatial_id);       // nuh_layer_id
  AppendBits(3, temporal_id + 1);  // nuh_temporal_id_plus_1
}

void H265AnnexBBitstreamBuilder::FinishNALU() {
  // RBSP stop one bit.
  AppendBits(1, 1);

  // Byte-alignment zero bits.
  AppendBits(bits_left_in_reg_ % 8, 0);

  Flush();
  in_nalu_ = false;
}

void H265AnnexBBitstreamBuilder::Flush() {
  if (bits_left_in_reg_ != kRegBitSize) {
    FlushReg();
  }
}

size_t H265AnnexBBitstreamBuilder::BitsInBuffer() const {
  return bits_in_buffer_;
}

size_t H265AnnexBBitstreamBuilder::BytesInBuffer() const {
  DCHECK_FINISHED();
  return data_.size();
}

rtc::ArrayView<const uint8_t> H265AnnexBBitstreamBuilder::data() const {
  RTC_DCHECK(!data_.empty());
  DCHECK_FINISHED();

  return data_;
}

}  // namespace webrtc
