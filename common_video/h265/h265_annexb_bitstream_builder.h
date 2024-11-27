/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H265_H265_ANNEXB_BITSTREAM_BUILDER_H_
#define COMMON_VIDEO_H265_H265_ANNEXB_BITSTREAM_BUILDER_H_

#include <stdint.h>

#include "api/array_view.h"
#include "common_video/h265/h265_common.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Holds one or more H.265 NALUs as a raw bitstream buffer in Annex-B format.
// Emulation prevention bytes are added when necessary.
class RTC_EXPORT H265AnnexBBitstreamBuilder {
 public:
  explicit H265AnnexBBitstreamBuilder(
      bool insert_emulation_prevention_bytes = false);
  ~H265AnnexBBitstreamBuilder();

  H265AnnexBBitstreamBuilder(const H265AnnexBBitstreamBuilder&) = delete;
  H265AnnexBBitstreamBuilder& operator=(const H265AnnexBBitstreamBuilder&) =
      delete;

  // Discard all data and reset the buffer for reuse.
  void Reset();

  // Append |num_bits| bits from |val| to the bitstream buffer.
  template <typename T>
  void AppendBits(size_t num_bits, T val) {
    AppendU64(num_bits, static_cast<uint64_t>(val));
  }

  void AppendBits(size_t num_bits, bool val) {
    RTC_DCHECK_EQ(num_bits, 1ul);
    AppendBool(val);
  }

  // Append a one-bit bool/flag value |val| to the bitstream buffer.
  void AppendBool(bool val);

  // Append a signed value in |val| in Exp-Golomb code to the bitstream buffer.
  void AppendSE(int val);

  // Append an unsigned value in |val| in Exp-Golomb code to the bitstream
  // buffer.
  void AppendUE(unsigned int val);

  // Start a H.265 NALU. Note that until FinishNALU is called, some of the bits
  // may not be flushed into the buffer and the data will not be correctly
  // aligned with trailing bits.
  void BeginNALU(H265::NaluType nalu_type,
                 uint8_t spatial_id,
                 uint8_t temporal_id);

  // Finish current NALU. This will flush any cached bits and correctly align
  // the buffer with RBSP trailing bits. This MUST be called for the stream
  // returned by data() to be correct.
  void FinishNALU();

  // Finish current bitstream. This will flush any cached bits in the reg
  // without RBSP trailing bits alignment. This can be called when RBSP trailing
  // will be added later. This must be called for the bistream returned by
  // data() to be correct.
  void Flush();

  // Flush any cached bits in the reg with byte granularity, i.e. enough bytes
  // to flush all pending bits, but not more.
  void FlushReg();

  // Return number of full bytes in the bitstream. Note that FinishNALU() has to
  // be called to flush cached bits, or the return value will not include them.
  size_t BytesInBuffer() const;

  // Return number of full bytes in the bitstream. Note that FinishNALU() has to
  // be called to flush cached bits, or the return value will not include them.
  size_t BitsInBuffer() const;

  // Returns the bitstream buffer.
  rtc::ArrayView<const uint8_t> data() const;

 private:
  // Allocate additional memory (kGrowBytes bytes) for the bitstream buffer.
  void Grow();

  // Append |num_bits| bits from |val| to the bitstream buffer.
  void AppendU64(size_t num_bits, uint64_t val);

  typedef uint64_t RegType;
  enum {
    // Sizes of reg_.
    kRegByteSize = sizeof(RegType),
    kRegBitSize = kRegByteSize * 8,
    // Amount of bytes to grow the buffer by when we run out of
    // previously-allocated memory for it.
    kGrowBytes = 4096,
  };

  static_assert(kGrowBytes >= kRegByteSize,
                "kGrowBytes must be larger than kRegByteSize");

  // Whether to insert emulation prevention bytes in RBSP.
  bool insert_emulation_prevention_bytes_ = false;

  // Whether BeginNALU() has been called but not FinishNALU().
  bool in_nalu_;

  // Unused bits left in reg_.
  size_t bits_left_in_reg_;

  // Cache for appended bits. Bits are flushed to data_ with kRegByteSize
  // granularity, i.e. when reg_ becomes full, or when an explicit FlushReg()
  // is called.
  RegType reg_;

  // Current byte offset in data_ (points to the start of unwritten bits).
  size_t pos_;
  // Current last bit in data_ (points to the start of unwritten bit).
  size_t bits_in_buffer_;

  // Buffer for stream data.
  rtc::Buffer data_;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_H265_H265_ANNEXB_BITSTREAM_BUILDER_H_
