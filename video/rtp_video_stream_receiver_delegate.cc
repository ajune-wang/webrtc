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

#include "modules/video_coding/frame_object.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "video/rtp_video_stream_receiver.h"

namespace webrtc {

RtpVideoStreamReceiverDelegate::RtpVideoStreamReceiverDelegate(
    rtc::WeakPtr<RtpVideoStreamReceiver> receiver,
    rtc::Thread* network_thread)
    : receiver_(receiver), network_thread_(network_thread) {}

void RtpVideoStreamReceiverDelegate::OnTransformedFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  network_thread_->PostTask(
      ToQueuedTask([this, frame = std::move(frame)]() mutable {
        if (!receiver_)
          return;
        auto transformed_frame = std::unique_ptr<video_coding::RtpFrameObject>(
            static_cast<video_coding::RtpFrameObject*>(frame.release()));
        receiver_->ManageFrame(std::move(transformed_frame));
      }));
}

}  // namespace webrtc
