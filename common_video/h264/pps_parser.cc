/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/pps_parser.h"

#include <cstdint>
#include <vector>

#include "common_video/h264/h264_common.h"
#include "rtc_base/checks.h"
#include "rtc_base/memory/bit_reader.h"

namespace webrtc {

namespace {
constexpr int kMaxPicInitQpDeltaValue = 25;
constexpr int kMinPicInitQpDeltaValue = -26;
}  // namespace


// General note: this is based off the 02/2014 version of the H.264 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.264

absl::optional<PpsParser::PpsState> PpsParser::ParsePps(const uint8_t* data,
                                                        size_t length) {
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1 of the H.264 standard.
  return ParseInternal(H264::ParseRbsp(data, length));
}

bool PpsParser::ParsePpsIds(const uint8_t* data,
                            size_t length,
                            uint32_t* pps_id,
                            uint32_t* sps_id) {
  RTC_DCHECK(pps_id);
  RTC_DCHECK(sps_id);
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1 of the H.264 standard.
  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(data, length);
  BitReader bit_reader(unpacked_buffer);
  *pps_id = bit_reader.ReadExponentialGolomb();
  *sps_id = bit_reader.ReadExponentialGolomb();
  return bit_reader.Ok();
}

absl::optional<uint32_t> PpsParser::ParsePpsIdFromSlice(const uint8_t* data,
                                                        size_t length) {
  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(data, length);
  BitReader slice_reader(unpacked_buffer);

  // first_mb_in_slice: ue(v)
  slice_reader.ReadExponentialGolomb();
  // slice_type: ue(v)
  slice_reader.ReadExponentialGolomb();
  // pic_parameter_set_id: ue(v)
  uint32_t slice_pps_id = slice_reader.ReadExponentialGolomb();
  if (!slice_reader.Ok()) {
    return absl::nullopt;
  }
  return slice_pps_id;
}

absl::optional<PpsParser::PpsState> PpsParser::ParseInternal(
    rtc::ArrayView<const uint8_t> buffer) {
  BitReader bit_reader(buffer);
  absl::optional<PpsState> pps(absl::in_place);

  // pic_parameter_set_id: ue(v)
  pps->id = bit_reader.ReadExponentialGolomb();
  // seq_parameter_set_id: ue(v)
  pps->sps_id = bit_reader.ReadExponentialGolomb();

  // entropy_coding_mode_flag: u(1)
  pps->entropy_coding_mode_flag = bit_reader.Read<bool>();
  // bottom_field_pic_order_in_frame_present_flag: u(1)
  pps->bottom_field_pic_order_in_frame_present_flag = bit_reader.Read<bool>();

  // num_slice_groups_minus1: ue(v)
  uint32_t num_slice_groups_minus1 = bit_reader.ReadExponentialGolomb();
  if (num_slice_groups_minus1 > 0) {
    // slice_group_map_type: ue(v)
    uint32_t slice_group_map_type = bit_reader.ReadExponentialGolomb();
    if (slice_group_map_type == 0) {
      for (uint32_t i_group = 0; i_group <= num_slice_groups_minus1;
           ++i_group) {
        // run_length_minus1[iGroup]: ue(v)
        bit_reader.ReadExponentialGolomb();
      }
    } else if (slice_group_map_type == 1) {
      // TODO(sprang): Implement support for dispersed slice group map type.
      // See 8.2.2.2 Specification for dispersed slice group map type.
    } else if (slice_group_map_type == 2) {
      for (uint32_t i_group = 0; i_group <= num_slice_groups_minus1;
           ++i_group) {
        // top_left[iGroup]: ue(v)
        bit_reader.ReadExponentialGolomb();
        // bottom_right[iGroup]: ue(v)
        bit_reader.ReadExponentialGolomb();
      }
    } else if (slice_group_map_type == 3 || slice_group_map_type == 4 ||
               slice_group_map_type == 5) {
      // slice_group_change_direction_flag: u(1)
      bit_reader.ConsumeBits(1);
      // slice_group_change_rate_minus1: ue(v)
      bit_reader.ReadExponentialGolomb();
    } else if (slice_group_map_type == 6) {
      // pic_size_in_map_units_minus1: ue(v)
      uint32_t pic_size_in_map_units_minus1 =
          bit_reader.ReadExponentialGolomb();
      uint32_t slice_group_id_bits = 0;
      uint32_t num_slice_groups = num_slice_groups_minus1 + 1;
      // If num_slice_groups is not a power of two an additional bit is required
      // to account for the ceil() of log2() below.
      if ((num_slice_groups & (num_slice_groups - 1)) != 0)
        ++slice_group_id_bits;
      while (num_slice_groups > 0) {
        num_slice_groups >>= 1;
        ++slice_group_id_bits;
      }
      for (uint32_t i = 0; i <= pic_size_in_map_units_minus1; i++) {
        // slice_group_id[i]: u(v)
        // Represented by ceil(log2(num_slice_groups_minus1 + 1)) bits.
        bit_reader.ConsumeBits(slice_group_id_bits);
      }
    }
  }
  // num_ref_idx_l0_default_active_minus1: ue(v)
  bit_reader.ReadExponentialGolomb();
  // num_ref_idx_l1_default_active_minus1: ue(v)
  bit_reader.ReadExponentialGolomb();
  // weighted_pred_flag: u(1)
  pps->weighted_pred_flag = bit_reader.Read<bool>();
  // weighted_bipred_idc: u(2)
  pps->weighted_bipred_idc = bit_reader.ReadBits(2);

  // pic_init_qp_minus26: se(v)
  pps->pic_init_qp_minus26 = bit_reader.ReadSignedExponentialGolomb();
  // Sanity-check parsed value
  if (pps->pic_init_qp_minus26 > kMaxPicInitQpDeltaValue ||
      pps->pic_init_qp_minus26 < kMinPicInitQpDeltaValue) {
    pps = absl::nullopt;
    return pps;
  }
  // pic_init_qs_minus26: se(v)
  bit_reader.ReadExponentialGolomb();
  // chroma_qp_index_offset: se(v)
  bit_reader.ReadExponentialGolomb();
  // deblocking_filter_control_present_flag: u(1)
  // constrained_intra_pred_flag: u(1)
  bit_reader.ConsumeBits(2);
  // redundant_pic_cnt_present_flag: u(1)
  pps->redundant_pic_cnt_present_flag = bit_reader.ReadBit();

  if (!bit_reader.Ok()) {
    pps = absl::nullopt;
  }
  return pps;
}

}  // namespace webrtc
