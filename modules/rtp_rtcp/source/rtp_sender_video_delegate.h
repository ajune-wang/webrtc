/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_DELEGATE_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_DELEGATE_H_

#include <memory>

#include "api/frame_transformer_interface.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class TransformableEncodedFrame;
class RTPSenderVideo;

// Delegates calls to RTPSenderVideo to send transformed frames. Ensures
// thread-safe access to the sender.
class RTPSenderVideoDelegate : public TransformedFrameCallback {
 public:
  RTPSenderVideoDelegate(RTPSenderVideo* sender, TaskQueueBase* encoder_queue);

  // Implements TransformedFrameCallback. Can be called on any thread. Posts
  // the transformed frame to be sent on the |encoded_queue_|.
  void OnTransformedFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) override;

  // Delegates the call to RTPSendVideo::SendVideo on the |encoded_queue_|.
  void SendVideo(TransformableEncodedFrame* transformed_frame);

  // Resets |sender_|. Called from RTPSenderVideo destructor to prevent the
  // |sender_| to dangle.
  void ResetSenderPtr();

 protected:
  ~RTPSenderVideoDelegate() override = default;

 private:
  TaskQueueBase* encoder_queue_;
  rtc::CriticalSection sender_lock_;
  RTPSenderVideo* sender_ RTC_GUARDED_BY(sender_lock_);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_DELEGATE_H_
