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

class RtpEncodedFrameObject;
class RTPSenderVideo;

class RTPSenderVideoDelegate : public TransformedFrameCallback {
 public:
  RTPSenderVideoDelegate(RTPSenderVideo* sender, TaskQueueBase* encoder_queue);

  // Implements TransformedFrameCallback.
  void OnTransformedFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) override;

  void SendVideo(RtpEncodedFrameObject* transformed_frame);
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
