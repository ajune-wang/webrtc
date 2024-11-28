/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_frame_builder.h"

#include <stdint.h>

#include "common_video/h265/h265_annexb_bitstream_builder.h"
#include "common_video/h265/h265_common.h"
#include "rtc_base/checks.h"

namespace webrtc {

void BuildKeyFrame(H265AnnexBBitstreamBuilder& builder,
                   size_t width,
                   size_t height,
                   uint8_t num_temporal_layers,
                   int qp,
                   size_t frame_size_bytes) {
  RTC_CHECK_GE(num_temporal_layers, 1u);
  RTC_CHECK_GE(qp, 0u);
  RTC_CHECK_GT(frame_size_bytes, 0ul);
  RTC_CHECK_GT(width, 0ul);
  RTC_CHECK_GT(height, 0ul);

  builder.Reset();
  // VPS NALU
  builder.BeginNALU(H265::NaluType::kVps, 0, 0);
  builder.AppendBits(4, 0);  // vps_video_parameter_set_id
  builder.AppendBits(1, 1);  // vps_base_layer_internal_flag
  builder.AppendBits(1, 1);  // vps_base_layer_available_flag
  builder.AppendBits(6, 0);  // vps_max_layers_minus1
  builder.AppendBits(3, num_temporal_layers - 1);  // vps_max_sub_layers_minus1
  builder.AppendBits(1, 1);        // vps_temporal_id_nesting_flag
  builder.AppendBits(16, 0xffff);  // vps_reserved_0xffff_16bits
  // profile/tier/level information
  builder.AppendBits(2, 0);  // general_profile_space
  builder.AppendBits(1, 0);  // general_tier_flag
  builder.AppendBits(5, 1);  // general_profile_idc
  builder.AppendBits(
      32, 0x40000000);        // general_profile_compatibility_flag[1] set to 1
  builder.AppendBits(1, 1);   // general_progressive_source_flag
  builder.AppendBits(1, 0);   // general_interlaced_source_flag
  builder.AppendBits(1, 0);   // general_non_packed_constraint_flag
  builder.AppendBits(1, 1);   // general_frame_only_constraint_flag
  builder.AppendBits(16, 0);  // general_reserved_zero_43bits[0-15]
  builder.AppendBits(16, 0);  // general_reserved_zero_43bits[16-31]
  builder.AppendBits(11, 0);  // general_reserved_zero_43bits[32-43]
  builder.AppendBits(1, 0);   // general_inbld_flag
  // We may consider pass the level from encoder. Currently fixed to level 5.
  builder.AppendBits(8, 150);  // general_level_idc
  for (int i = 0; i < num_temporal_layers - 1; ++i) {
    builder.AppendBits(1, 0);  // sub_layer_profile_present_flag[i]
    builder.AppendBits(1, 0);  // sub_layer_level_present_flag[i]
  }
  if (num_temporal_layers > 1) {
    for (int i = num_temporal_layers - 1; i < 8; ++i) {
      builder.AppendBits(2, 0);  // reserved_zero_2bits[i]
    }
  }
  // Additional VPS parameters
  builder.AppendBits(1, 0);  // vps_sub_layer_ordering_info_present_flag
  // Since we fix vps_sub_layer_ordering_info_present_flag to 0, we will only
  // have one set of vps_max_dec_pic_buffering_minus1/vps_max_num_reorder_pics
  // and vps_max_latency_increase_plus1 for all sub-layers.
  builder.AppendUE(1);       // vps_max_dec_pic_buffering_minus1
  builder.AppendUE(0);       // vps_max_num_reorder_pics
  builder.AppendUE(0);       // vps_max_latency_increase_plus1
  builder.AppendBits(6, 0);  // vps_max_layer_id
  builder.AppendUE(0);       // vps_num_layer_sets_minus1

  builder.AppendBits(1, 1);    // vps_timing_info_present_flag
  builder.AppendBits(32, 1);   // vps_num_units_in_tick
  builder.AppendBits(32, 30);  // vps_time_scale
  builder.AppendBits(1, 0);    // vps_poc_proportional_to_timing_flag
  builder.AppendUE(0);         // vps_num_hrd_parameters
  builder.AppendBits(1, 0);    // vps_extension_flag
  builder.FinishNALU();

  // SPS NALU
  builder.BeginNALU(H265::NaluType::kSps, 0, 0);
  builder.AppendBits(4, 0);                        // sps_video_parameter_set_id
  builder.AppendBits(3, num_temporal_layers - 1);  // sps_max_sub_layers_minus1
  builder.AppendBits(1, 1);  // sps_temporal_id_nesting_flag

  // profile/tier/level information
  builder.AppendBits(2, 0);  // general_profile_space
  builder.AppendBits(1, 0);  // general_tier_flag
  builder.AppendBits(5, 1);  // general_profile_idc
  builder.AppendBits(
      32, 0x40000000);        // general_profile_compatibility_flag[1] set to 1
  builder.AppendBits(1, 1);   // general_progressive_source_flag
  builder.AppendBits(1, 0);   // general_interlaced_source_flag
  builder.AppendBits(1, 0);   // general_non_packed_constraint_flag
  builder.AppendBits(1, 1);   // general_frame_only_constraint_flag
  builder.AppendBits(16, 0);  // general_reserved_zero_43bits[0-15]
  builder.AppendBits(16, 0);  // general_reserved_zero_43bits[16-31]
  builder.AppendBits(11, 0);  // general_reserved_zero_43bits[32-43]
  builder.AppendBits(1, 0);   // general_inbld_flag
  // We may consider pass the level from encoder. Currently fixed to level 5.
  builder.AppendBits(8, 150);  // general_level_idc
  for (int i = 0; i < num_temporal_layers - 1; ++i) {
    builder.AppendBits(1, 0);  // sub_layer_profile_present_flag[i]
    builder.AppendBits(1, 0);  // sub_layer_level_present_flag[i]
  }
  if (num_temporal_layers > 1) {
    for (int i = num_temporal_layers - 1; i < 8; ++i) {
      builder.AppendBits(2, 0);  // reserved_zero_2bits[i]
    }
  }

  builder.AppendUE(0);       // sps_seq_parameter_set_id
  builder.AppendUE(1);       // chroma_format_idc = 1,YUV 4:2:0
  builder.AppendUE(width);   // pic_width_in_luma_samples
  builder.AppendUE(height);  // pic_height_in_luma_samples
  builder.AppendBits(1, 0);  // conformance_window_flag
  builder.AppendUE(0);       // bit_depth_luma_minus8
  builder.AppendUE(0);       // bit_depth_chroma_minus8
  builder.AppendUE(0);       // log2_max_pic_order_cnt_lsb_minus4
  builder.AppendBits(1, 0);  // sps_sub_layer_ordering_info_present_flag

  builder.AppendUE(1);  // sps_max_dec_pic_buffering_minus1
  builder.AppendUE(0);  // sps_max_num_reorder_pics
  builder.AppendUE(0);  // sps_max_latency_increase_plus1

  builder.AppendUE(0);       // log2_min_luma_coding_block_size_minus3
  builder.AppendUE(3);       // log2_diff_max_min_luma_coding_block_size
  builder.AppendUE(0);       // log2_min_luma_transform_block_size_minus2
  builder.AppendUE(3);       // log2_diff_max_min_luma_transform_block_size
  builder.AppendUE(2);       // max_transform_hierarchy_depth_inter
  builder.AppendUE(2);       // max_transform_hierarchy_depth_intra
  builder.AppendBits(1, 0);  // scaling_list_enabled_flag
  builder.AppendBits(1, 1);  // amp_enabled_flag
  builder.AppendBits(1, 1);  // sample_adaptive_offset_enabled_flag
  builder.AppendBits(1, 0);  // pcm_enabled_flag
  builder.AppendUE(2);       // num_short_term_ref_pic_sets

  // Rps at index 0
  builder.AppendUE(1);       // num_negative_pics
  builder.AppendUE(0);       // num_positive_pics
  builder.AppendUE(0);       // delta_poc_s0_minus1
  builder.AppendBits(1, 0);  // used_by_curr_pic_s0_flag
  // Rps at index 1
  builder.AppendBits(1, 1);  // inter_ref_pic_set_prediction_flag
  builder.AppendBits(1, 1);  // delta_rps_sign
  builder.AppendUE(0);       // abs_delta_rps_minus1
  builder.AppendBits(1, 1);  // used_by_curr_pic_flag[0]
  builder.AppendBits(1, 0);  // used_by_curr_pic_flag[1]
  builder.AppendBits(1, 0);  // use_delta_flag[1]

  builder.AppendBits(1, 1);  // long_term_ref_pics_present_flag
  builder.AppendUE(0);       // num_long_term_ref_pics_sps

  builder.AppendBits(1, 1);  // sps_temporal_mvp_enabled_flag
  builder.AppendBits(1, 0);  // strong_intra_smoothing_enabled_flag
  builder.AppendBits(1, 0);  // vui_parameters_present_flag
  builder.AppendBits(1, 0);  // sps_extension_flag
  builder.FinishNALU();

  // PPS NALU
  builder.BeginNALU(H265::NaluType::kPps, 0, 0);
  builder.AppendUE(0);       // pps_pic_parameter_set_id
  builder.AppendUE(0);       // pps_seq_parameter_set_id
  builder.AppendBits(1, 0);  // dependent_slice_segments_enabled_flag
  builder.AppendBits(1, 0);  // output_flag_present_flag
  builder.AppendBits(3, 0);  // num_extra_slice_header_bits
  builder.AppendBits(1, 0);  // sign_data_hiding_enabled_flag
  builder.AppendBits(1, 0);  // cabac_init_present_flag
  builder.AppendUE(0);       // num_ref_idx_l0_default_active_minus1
  builder.AppendUE(0);       // num_ref_idx_l1_default_active_minus1
  builder.AppendSE(0);       // init_qp_minus26
  builder.AppendBits(1, 0);  // constrained_intra_pred_flag
  builder.AppendBits(1, 1);  // transform_skip_enabled_flag
  builder.AppendBits(1, 1);  // cu_qp_delta_enabled_flag
  builder.AppendUE(3);       // diff_cu_qp_delta_depth
  builder.AppendSE(0);       // pps_cb_qp_offset
  builder.AppendSE(0);       // pps_cr_qp_offset
  builder.AppendBits(1, 0);  // pps_slice_chroma_qp_offsets_present_flag
  builder.AppendBits(1, 0);  // weighted_pred_flag
  builder.AppendBits(1, 0);  // weighted_bipred_flag
  builder.AppendBits(1, 0);  // transquant_bypass_enabled_flag
  builder.AppendBits(1, 0);  // tiles_enabled_flag
  builder.AppendBits(1, 0);  // entropy_coding_sync_enabled_flag
  builder.AppendBits(1, 1);  // loop_filter_across_tiles_enabled_flag
  builder.AppendBits(1, 0);  // pps_loop_filter_across_slices_enabled_flag
  builder.AppendBits(1, 0);  // deblocking_filter_control_present_flag
  builder.AppendBits(1, 0);  // pps_scaling_list_data_present_flag
  builder.AppendBits(1, 0);  // lists_modification_present_flag
  builder.AppendUE(0);       // log2_parallel_merge_level_minus2
  builder.AppendBits(1, 0);  // slice_segment_header_extension_present_flag
  builder.AppendBits(1, 0);  // pps_extension_flag
  builder.FinishNALU();

  // IDR_W_RADL NALU
  builder.BeginNALU(H265::NaluType::kIdrWRadl, 0, 0);
  builder.AppendBits(1, 1);   // first_slice_segment_in_pic_flag
  builder.AppendBits(1, 0);   // no_output_of_prior_pics_flag
  builder.AppendUE(0);        // slice_pic_parameter_set_id
  builder.AppendUE(2);        // slice_type (I slice)
  builder.AppendBits(1, 1);   // slice_sao_luma_flag
  builder.AppendBits(1, 1);   // slice_sao_chroma_flag
  builder.AppendSE(qp - 26);  // slice_qp_delta
  builder.AppendBits(1, 0);   // deblocking_filter_override_flag
  builder.AppendBits(1, 0);   // slice_loop_filter_across_slices_enabled_flag

  while (builder.BitsInBuffer() % 8 != 0) {
    builder.AppendBits(1, 0);  // alignment bits
  }

  // Fill the rest of the IDR_W_RADL NALU with dummy data to match the frame
  // size
  size_t current_size = builder.BitsInBuffer() % 8;
  if (current_size < frame_size_bytes) {
    // Consider the emulation prevention bytes inserted due to 00 00 pattern.
    size_t remaining_size = (frame_size_bytes - current_size) * 2 / 3;
    for (size_t i = 0; i < remaining_size; ++i) {
      builder.AppendBits(8, 0);  // Append zero bytes
    }
  }

  builder.FinishNALU();
}

RTC_EXPORT void BuildDeltaFrame(H265AnnexBBitstreamBuilder& builder,
                                uint8_t temporal_layer_id,
                                int qp,
                                size_t frame_size_bytes,
                                uint8_t wrapped_on_16_poc_lsb) {
  RTC_CHECK_GE(qp, 0u);
  RTC_CHECK_GE(qp, 0u);
  RTC_CHECK_GT(frame_size_bytes, 0ul);
  RTC_CHECK_LE(wrapped_on_16_poc_lsb, 15u);

  builder.Reset();
  builder.BeginNALU(H265::NaluType::kTrailN, 0, temporal_layer_id);
  builder.AppendBits(1, 1);  // first_slice_segment_in_pic_flag
  builder.AppendUE(0);       // slice_pic_parameter_set_id
  builder.AppendUE(0);       // slice_type (hierarchical B slice)
  builder.AppendBits(4, wrapped_on_16_poc_lsb);  // slice_pic_order_cnt_lsb
  builder.AppendBits(1, 1);  // short_term_ref_pic_set_sps_flag
  builder.AppendBits(
      1, (temporal_layer_id + 1) % 2);  // short_term_ref_pic_set_idx
  builder.AppendUE(0);                  // num_long_term_pics
  builder.AppendBits(1, 1);             // slice_temporal_mvp_enabled_flag
  builder.AppendBits(1, 1);             // slice_sao_luma_flag
  builder.AppendBits(1, 1);             // slice_sao_chroma_flag
  builder.AppendBits(1, 0);             // num_ref_idx_active_override_flag
  builder.AppendBits(1, 0);             // mvd_l1_zero_flag
  builder.AppendBits(1, 1);             // colloacted_from_l0_flag
  builder.AppendUE(0);                  // five_minus_max_num_merge_cand
  builder.AppendSE(qp - 26);            // slice_qp_delta
  builder.AppendBits(1, 0);             // deblocking_filter_override_flag
  builder.AppendBits(1, 1);  // slice_loop_filter_across_slices_enabled_flag

  while (builder.BitsInBuffer() % 8 != 0) {
    builder.AppendBits(1, 0);  // alignment bits
  }

  // Fill the rest of the TRAIL_N NALU with dummy data to match the frame size.
  size_t current_size = builder.BitsInBuffer() % 8;
  if (current_size < frame_size_bytes) {
    // Consider the emulation prevention bytes inserted due to 00 00 pattern.
    size_t remaining_size = (frame_size_bytes - current_size) * 2 / 3;
    for (size_t i = 0; i < remaining_size; ++i) {
      builder.AppendBits(8, 0);  // Append zero bytes
    }
  }

  builder.FinishNALU();
}

}  // namespace webrtc
