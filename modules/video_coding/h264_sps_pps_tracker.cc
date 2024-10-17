/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h264_sps_pps_tracker.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/types/variant.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace video_coding {

H264SpsPpsTracker::PacketAction H264SpsPpsTracker::Track(
    rtc::ArrayView<const uint8_t> bitstream,
    RTPVideoHeader* video_header) {
  RTC_DCHECK(video_header);
  RTC_DCHECK(video_header->codec == kVideoCodecH264);
  RTC_DCHECK_GT(bitstream.size(), 0);

  H264SpsPpsTracker::PacketAction action = kInsert;

  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(video_header->video_type_header);

  auto sps = sps_ids_.end();
  auto pps = pps_to_sps_ids_.end();

  for (const NaluInfo& nalu : h264_header.nalus) {
    switch (nalu.type) {
      case H264::NaluType::kSps: {
        sps_ids_.insert(nalu.sps_id);
        break;
      }
      case H264::NaluType::kPps: {
        pps_to_sps_ids_[nalu.pps_id] = nalu.sps_id;
        break;
      }
      case H264::NaluType::kIdr: {
        // If this is the first packet of an IDR, make sure we have the required
        // SPS/PPS and also calculate how much extra space we need in the buffer
        // to prepend the SPS/PPS to the bitstream with start codes.
        if (video_header->is_first_packet_in_frame) {
          if (nalu.pps_id == -1) {
            RTC_LOG(LS_WARNING) << "No PPS id in IDR nalu.";
            return kRequestKeyframe;
          }

          pps = pps_to_sps_ids_.find(nalu.pps_id);
          if (pps == pps_to_sps_ids_.end()) {
            RTC_LOG(LS_WARNING)
                << "No PPS with id << " << nalu.pps_id << " received";
            return kRequestKeyframe;
          }

          sps = sps_ids_.find(pps->second);
          if (sps == sps_ids_.end()) {
            RTC_LOG(LS_WARNING)
                << "No SPS with id << " << pps->second << " received";
            return kRequestKeyframe;
          }
        }
        break;
      }
      default:
        break;
    }
  }

  return action;
}

}  // namespace video_coding
}  // namespace webrtc
