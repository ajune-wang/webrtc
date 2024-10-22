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

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/buffer.h"
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
  struct PpsInfo {
    int sps_id = -1;
    rtc::Buffer data;
  };

  struct SpsInfo {
    int width = 0;
    int height = 0;
    rtc::Buffer data;
  };

  std::optional<VideoRtpDepacketizer::ParsedRtpPayload>
  ProcessStapAOrSingleNalu(rtc::CopyOnWriteBuffer rtp_payload);

  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ParseFuaNalu(
      rtc::CopyOnWriteBuffer rtp_payload);

  std::optional<std::pair<VideoRtpDepacketizerH264::SpsInfo&,
                          VideoRtpDepacketizerH264::PpsInfo&>>
  GetSpsPpsInfo(uint8_t pic_parameter_set_id);

  bool CheckAndMaybeInsertSpsPps(uint8_t pic_parameter_set_id,
                                 rtc::ByteBufferWriter& writer);

  std::map<int, PpsInfo> pps_data_;
  std::map<int, SpsInfo> sps_data_;

  bool has_out_of_band_sps_pps_ = false;
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_
