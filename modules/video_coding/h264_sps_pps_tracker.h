/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_H264_SPS_PPS_TRACKER_H_
#define MODULES_VIDEO_CODING_H264_SPS_PPS_TRACKER_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/buffer.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {
namespace video_coding {

class H264SpsPpsTracker {
 public:
  enum PacketAction { kInsert, kDrop, kRequestKeyframe };

  H264SpsPpsTracker() = default;
  H264SpsPpsTracker(const H264SpsPpsTracker& other) = default;
  H264SpsPpsTracker& operator=(const H264SpsPpsTracker& other) = default;
  ~H264SpsPpsTracker() = default;

  // Checks pps/sps and updates `video_header`.
  PacketAction Track(rtc::ArrayView<const uint8_t> bitstream,
                     RTPVideoHeader* video_header);

 private:
  std::map<int, int> pps_to_sps_ids_;
  std::set<int> sps_ids_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_H264_SPS_PPS_TRACKER_H_
