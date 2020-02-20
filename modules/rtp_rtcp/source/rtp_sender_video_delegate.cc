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
#include <memory>

#include "modules/rtp_rtcp/source/rtp_encoded_frame_object.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

RTPSenderVideoDelegate::RTPSenderVideoDelegate(RTPSenderVideo* sender,
                                               TaskQueueBase* encoder_queue)
    : encoder_queue_(encoder_queue) {
  sender_ = sender;
}

void RTPSenderVideoDelegate::OnTransformedFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  {
    rtc::CritScope lock(&sender_lock_);
    if (!sender_)
      return;
  }
  auto frame_object = std::unique_ptr<RtpEncodedFrameObject>(
      static_cast<RtpEncodedFrameObject*>(frame.release()));
  rtc::scoped_refptr<RTPSenderVideoDelegate> delegate = this;
  encoder_queue_->PostTask(
      ToQueuedTask([delegate = std::move(delegate),
                    transformed_frame = std::move(frame_object)]() {
        delegate->SendVideo(transformed_frame.get());
      }));
}

void RTPSenderVideoDelegate::SendVideo(
    RtpEncodedFrameObject* transformed_frame) {
  rtc::CritScope lock(&sender_lock_);
  if (!sender_)
    return;
  sender_->DoSendVideo(
      transformed_frame->PayloadType(), transformed_frame->codec_type(),
      transformed_frame->Timestamp(), transformed_frame->capture_time_ms(),
      transformed_frame->EncodedImage(),
      transformed_frame->fragmentation_header(),
      transformed_frame->video_header(),
      transformed_frame->expected_retransmission_time_ms());
}

void RTPSenderVideoDelegate::ResetSenderPtr() {
  rtc::CritScope lock(&sender_lock_);
  sender_ = nullptr;
}
}  // namespace webrtc
