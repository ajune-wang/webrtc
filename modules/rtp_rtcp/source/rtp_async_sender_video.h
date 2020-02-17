/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_ASYNC_SENDER_VIDEO_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_ASYNC_SENDER_VIDEO_H_

#include "modules/rtp_rtcp/source/rtp_sender_video.h"

namespace webrtc {

class RTPAsyncSenderVideo : public RTPSenderVideo,
                            public
    : RTPAsyncSenderVideo(
          const RTPSenderVideo::Config& config,
          rtc::scoped_refptr<FrameTransformerInterface> frame_transformer);
~RTPAsyncSenderVideo();

bool SendVideo(
    int payload_type,
    absl::optional<VideoCodecType> codec_type,
    uint32_t rtp_timestamp,
    int64_t capture_time_ms,
    const EncodedImage& encoded_image,
    const RTPFragmentationHeader* fragmentation,
    RTPVideoHeader video_header,
    absl::optional<int64_t> expected_retransmission_time_ms) override;

private:
};  // namespace webrtc

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_ASYNC_SENDER_VIDEO_H_
