/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_
#define MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_

#include <optional>
#include <vector>

#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {
class VideoRtpDepacketizerH264 : public VideoRtpDepacketizer {
 public:
  ~VideoRtpDepacketizerH264() override = default;

  std::optional<ParsedRtpPayload> Parse(
      rtc::CopyOnWriteBuffer rtp_payload) override;

  rtc::scoped_refptr<EncodedImageBuffer> AssembleFrame(
      rtc::ArrayView<const rtc::ArrayView<const uint8_t>> rtp_payloads)
      override;

  void InsertSpsPpsNalus(const std::vector<uint8_t>& sps,
                         const std::vector<uint8_t>& pps);

 private:
  std::vector<rtc::Buffer> pps_data_;
  std::vector<rtc::Buffer> sps_data_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_
