/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h265_parameter_sets_tracker.h"

#include <memory>
#include <utility>
#include <vector>

#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_pps_parser.h"
#include "common_video/h265/h265_sps_parser.h"
#include "common_video/h265/h265_vps_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace video_coding {

H265ParameterSetsTracker::H265ParameterSetsTracker() = default;
H265ParameterSetsTracker::~H265ParameterSetsTracker() = default;

H265ParameterSetsTracker::PpsInfo::PpsInfo() = default;
H265ParameterSetsTracker::PpsInfo::PpsInfo(PpsInfo&& rhs) = default;
H265ParameterSetsTracker::PpsInfo& H265ParameterSetsTracker::PpsInfo::operator=(
    PpsInfo&& rhs) = default;
H265ParameterSetsTracker::PpsInfo::~PpsInfo() = default;

H265ParameterSetsTracker::SpsInfo::SpsInfo() = default;
H265ParameterSetsTracker::SpsInfo::SpsInfo(SpsInfo&& rhs) = default;
H265ParameterSetsTracker::SpsInfo& H265ParameterSetsTracker::SpsInfo::operator=(
    SpsInfo&& rhs) = default;
H265ParameterSetsTracker::SpsInfo::~SpsInfo() = default;

H265ParameterSetsTracker::VpsInfo::VpsInfo() = default;
H265ParameterSetsTracker::VpsInfo::VpsInfo(VpsInfo&& rhs) = default;
H265ParameterSetsTracker::VpsInfo& H265ParameterSetsTracker::VpsInfo::operator=(
    VpsInfo&& rhs) = default;
H265ParameterSetsTracker::VpsInfo::~VpsInfo() = default;

H265ParameterSetsTracker::FixedBitstream
H265ParameterSetsTracker::MaybeFixBitstream(
    rtc::ArrayView<const uint8_t> bitstream) {
  if (!bitstream.size()) {
    return {kPassThrough};
  }

  // First AUD NALU should be dropped from the bitstream, as VideoToolBox
  // decoder does not handle it well (similar as webrtc:6173).
  bool has_aud_nalu = false;
  size_t aud_size = 0;
  bool append_vps = true, append_sps = true, append_pps = true;

  // Required size of fixed bitstream.
  size_t required_size = 0;
  H265ParameterSetsTracker::FixedBitstream fixed;
  auto vps_info = vps_data_.end();
  auto sps_info = sps_data_.end();
  auto pps_info = pps_data_.end();
  absl::optional<uint32_t> pps_id;
  int sps_id = -1, vps_id = -1;
  uint32_t slice_sps_id = 0, slice_pps_id = 0;

  parser_.ParseBitstream(
      rtc::ArrayView<const uint8_t>(bitstream.data(), bitstream.size()));

  std::vector<webrtc::H265::NaluIndex> nalu_indices =
      webrtc::H265::FindNaluIndices(bitstream.data(), bitstream.size());
  for (const auto& nalu_index : nalu_indices) {
    if (nalu_index.payload_size < 2) {
      // H.265 NALU header is at least 2 bytes.
      return {kRequestKeyframe};
    }
    const uint8_t* payload_start =
        bitstream.data() + nalu_index.payload_start_offset;
    const uint8_t* nalu_start = bitstream.data() + nalu_index.start_offset;
    size_t nalu_size = nalu_index.payload_size +
                       nalu_index.payload_start_offset -
                       nalu_index.start_offset;
    uint8_t nalu_type = H265::ParseNaluType(payload_start[0]);

    absl::optional<webrtc::H265VpsParser::VpsState> vps;
    absl::optional<webrtc::H265SpsParser::SpsState> sps;

    switch (nalu_type) {
      case webrtc::H265::NaluType::kAud:
        has_aud_nalu = true;
        aud_size = nalu_size;
        break;
      case webrtc::H265::NaluType::kVps:
        // H.265 parameter set parsers expect NALU header already stripped.
        vps = webrtc::H265VpsParser::ParseVps(payload_start + 2,
                                              nalu_index.payload_size - 2);
        // Always replace VPS with the same ID. Same for other parameter sets.
        if (vps) {
          VpsInfo current_vps_info;
          // Copy with start code included. Same for other parameter sets.
          current_vps_info.size = nalu_size;
          uint8_t* vps_data = new uint8_t[current_vps_info.size];
          memcpy(vps_data, nalu_start, current_vps_info.size);
          current_vps_info.data.reset(vps_data);
          vps_data_[vps->id] = std::move(current_vps_info);
        }
        append_vps = false;
        break;
      case webrtc::H265::NaluType::kSps:
        sps = webrtc::H265SpsParser::ParseSps(payload_start + 2,
                                              nalu_index.payload_size - 2);
        if (sps) {
          SpsInfo current_sps_info;
          current_sps_info.size = nalu_size;
          current_sps_info.vps_id = sps->sps_id;
          uint8_t* sps_data = new uint8_t[current_sps_info.size];
          memcpy(sps_data, nalu_start, current_sps_info.size);
          current_sps_info.data.reset(sps_data);
          sps_data_[sps->sps_id] = std::move(current_sps_info);
        }
        append_sps = false;
        break;
      case webrtc::H265::NaluType::kPps:
        if (webrtc::H265PpsParser::ParsePpsIds(payload_start + 2,
                                               nalu_index.payload_size - 2,
                                               &slice_pps_id, &slice_sps_id)) {
          auto current_sps_info = sps_data_.find(slice_sps_id);
          if (current_sps_info == sps_data_.end()) {
            RTC_LOG(LS_WARNING)
                << "No SPS associated with current parsed PPS found.";
            fixed.action = kRequestKeyframe;
          } else {
            PpsInfo current_pps_info;
            current_pps_info.size = nalu_size;
            current_pps_info.sps_id = slice_sps_id;
            uint8_t* pps_data = new uint8_t[current_pps_info.size];
            memcpy(pps_data, nalu_start, current_pps_info.size);
            current_pps_info.data.reset(pps_data);
            pps_data_[slice_pps_id] = std::move(current_pps_info);
          }
          append_pps = false;
        }
        break;
      case webrtc::H265::NaluType::kBlaWLp:
      case webrtc::H265::NaluType::kBlaWRadl:
      case webrtc::H265::NaluType::kBlaNLp:
      case webrtc::H265::NaluType::kIdrWRadl:
      case webrtc::H265::NaluType::kIdrNLp:
      case webrtc::H265::NaluType::kCra:
        pps_id = parser_.GetLastSlicePPSId();
        if (!pps_id) {
          RTC_LOG(LS_WARNING) << "Failed to parse PPS id from current slice.";
          fixed.action = kRequestKeyframe;
          break;
        }
        pps_info = pps_data_.find(pps_id.value());
        if (pps_info == pps_data_.end()) {
          RTC_LOG(LS_WARNING)
              << "PPS associated with current slice is not found.";
          fixed.action = kRequestKeyframe;
          break;
        }

        sps_id = pps_data_[pps_id.value()].sps_id;
        sps_info = sps_data_.find(sps_id);
        if (sps_info == sps_data_.end()) {
          RTC_LOG(LS_WARNING)
              << "SPS associated with current slice is not found.";
          fixed.action = kRequestKeyframe;
          break;
        }

        vps_id = sps_data_[sps_id].vps_id;
        vps_info = vps_data_.find(vps_id);
        if (vps_info == vps_data_.end()) {
          RTC_LOG(LS_WARNING)
              << "VPS associated with current slice is not found.";
          fixed.action = kRequestKeyframe;
          break;
        }

        if (!append_vps && !append_sps && !append_pps) {
          // No insertion of parameter sets needed.
          if (!has_aud_nalu) {
            fixed.action = kPassThrough;
          } else {
            fixed.action = kDropAud;
            fixed.bitstream.EnsureCapacity(required_size);
            fixed.bitstream.AppendData(bitstream.data() + aud_size,
                                       bitstream.size() - aud_size);
          }
        } else {
          required_size += vps_info->second.size + sps_info->second.size +
                           pps_info->second.size;

          required_size += bitstream.size();
          if (has_aud_nalu) {
            required_size -= aud_size;
          }

          fixed.bitstream.EnsureCapacity(required_size);
          fixed.bitstream.AppendData(vps_info->second.data.get(),
                                     vps_info->second.size);
          fixed.bitstream.AppendData(sps_info->second.data.get(),
                                     sps_info->second.size);
          fixed.bitstream.AppendData(pps_info->second.data.get(),
                                     pps_info->second.size);

          if (!has_aud_nalu) {
            fixed.bitstream.AppendData(bitstream.data(), bitstream.size());
            fixed.action = kInsert;
          } else {
            fixed.bitstream.AppendData(bitstream.data() + aud_size,
                                       bitstream.size() - aud_size);
            fixed.action = kInsertAndDropAud;
          }
        }
        break;
      default:
        break;
    }

    if (fixed.action == kRequestKeyframe) {
      return {kRequestKeyframe};
    } else if (fixed.action == kInsert || fixed.action == kInsertAndDropAud ||
               fixed.action == kDropAud) {
      return fixed;
    }
  }

  // Handle kDropAud of delta frames.
  if (has_aud_nalu) {
    required_size = bitstream.size() - aud_size;
    fixed.bitstream.EnsureCapacity(required_size);
    fixed.bitstream.AppendData(bitstream.data() + aud_size,
                               bitstream.size() - aud_size);
    fixed.action = kDropAud;
    return fixed;
  } else {
    return {kPassThrough};
  }
}

}  // namespace video_coding
}  // namespace webrtc
