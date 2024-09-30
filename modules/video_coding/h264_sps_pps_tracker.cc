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

#include "common_video/h264/h264_common.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
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

  auto sps = sps_data_.end();
  auto pps = pps_data_.end();

  for (const NaluInfo& nalu : h264_header.nalus) {
    switch (nalu.type) {
      case H264::NaluType::kSps: {
        SpsInfo& sps_info = sps_data_[nalu.sps_id];
        sps_info.width = video_header->width;
        sps_info.height = video_header->height;
        break;
      }
      case H264::NaluType::kPps: {
        pps_data_[nalu.pps_id].sps_id = nalu.sps_id;
        break;
      }
      case H264::NaluType::kIdr: {
        // If this is the first packet of an IDR, make sure we have the required
        // SPS/PPS
        if (video_header->is_first_packet_in_frame) {
          if (nalu.pps_id == -1) {
            RTC_LOG(LS_WARNING) << "No PPS id in IDR nalu.";
            return kRequestKeyframe;
          }

          pps = pps_data_.find(nalu.pps_id);
          if (pps == pps_data_.end()) {
            RTC_LOG(LS_WARNING)
                << "No PPS with id << " << nalu.pps_id << " received";
            return kRequestKeyframe;
          }

          sps = sps_data_.find(pps->second.sps_id);
          if (sps == sps_data_.end()) {
            RTC_LOG(LS_WARNING)
                << "No SPS with id << " << pps->second.sps_id << " received";
            return kRequestKeyframe;
          }

          // Since the first packet of every keyframe should have its width and
          // height set we set it here in the case of it being supplied out of
          // band.
          video_header->width = sps->second.width;
          video_header->height = sps->second.height;
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
