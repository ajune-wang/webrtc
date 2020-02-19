/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video_delegate.h"

#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "modules/video_coding/rtp_encoded_frame_object.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

RTPSenderVideoDelegate::RTPSenderVideoDelegate(
    rtc::WeakPtr<RTPSenderVideo> sender,
    TaskQueueBase* encoder_queue)
    : sender_(sender), encoder_queue_(encoder_queue) {}

void RTPSenderVideoDelegate::OnTransformedFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  auto sender = sender_;
  auto frame_object =
      std::unique_ptr<video_coding::RtpEncodedFrameObject>(
          static_cast<video_coding::RtpEncodedFrameObject*>(frame.release()));
  encoder_queue_->PostTask(ToQueuedTask([sender,
                                         transformed_frame = std::move(frame_object)]() {
     if (sender) {
       sender->DoSendVideo(
           transformed_frame->PayloadType(), transformed_frame->codec_type(),
           transformed_frame->Timestamp(), transformed_frame->capture_time_ms(),
           transformed_frame->EncodedImage(),
           transformed_frame->fragmentation_header(),
           transformed_frame->video_header(),
           transformed_frame->expected_retransmission_time_ms());
     }
  }));
}
}  // namespace webrtc
