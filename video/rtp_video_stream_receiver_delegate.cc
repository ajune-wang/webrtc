/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rtp_video_stream_receiver_delegate.h"

#include <utility>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread.h"
#include "video/rtp_video_stream_receiver.h"

namespace webrtc {

RtpVideoReceiverFrameTransformerDelegate::
    RtpVideoReceiverFrameTransformerDelegate(
        rtc::WeakPtr<RtpVideoStreamReceiver> receiver,
        rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
    : receiver_(receiver),
      frame_transformer_(std::move(frame_transformer)),
      network_thread_(rtc::Thread::Current()) {}

void RtpVideoReceiverFrameTransformerDelegate::Init() {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  frame_transformer_->RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>(this));
}

void RtpVideoReceiverFrameTransformerDelegate::Reset() {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  frame_transformer_->UnregisterTransformedFrameCallback();
  frame_transformer_ = nullptr;
}

void RtpVideoReceiverFrameTransformerDelegate::TransformFrame(
    std::unique_ptr<video_coding::RtpFrameObject> frame,
    uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  auto additional_data =
      RtpDescriptorAuthentication(frame->GetRtpVideoHeader());
  frame_transformer_->TransformFrame(std::move(frame),
                                     std::move(additional_data), ssrc);
}

void RtpVideoReceiverFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  rtc::scoped_refptr<RtpVideoReceiverFrameTransformerDelegate> delegate = this;
  network_thread_->PostTask(ToQueuedTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        delegate->ManageFrame(std::move(frame));
      }));
}

void RtpVideoReceiverFrameTransformerDelegate::ManageFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  if (!receiver_)
    return;
  auto transformed_frame = absl::WrapUnique(
      static_cast<video_coding::RtpFrameObject*>(frame.release()));
  receiver_->ManageFrame(std::move(transformed_frame));
}

}  // namespace webrtc
