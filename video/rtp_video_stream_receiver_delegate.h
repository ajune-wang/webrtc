/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_RTP_VIDEO_STREAM_RECEIVER_DELEGATE_H_
#define VIDEO_RTP_VIDEO_STREAM_RECEIVER_DELEGATE_H_

#include <memory>

#include "api/frame_transformer_interface.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class RtpVideoStreamReceiver;

// Delegates calls to RtpVideoStreamReceiver to manage transformed frames.
class RtpVideoStreamReceiverDelegate : public TransformedFrameCallback {
 public:
  RtpVideoStreamReceiverDelegate(rtc::WeakPtr<RtpVideoStreamReceiver> receiver,
                                 rtc::Thread* network_thread);

  // Implements TransformedFrameCallback. Can be called on any thread. Posts
  // the transformed frame to be managed on the |network_thread_|.
  void OnTransformedFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) override;

 protected:
  ~RtpVideoStreamReceiverDelegate() override = default;

 private:
  rtc::ThreadChecker network_thread_checker_;
  rtc::WeakPtr<RtpVideoStreamReceiver> receiver_
      RTC_PT_GUARDED_BY(network_thread_checker_);
  rtc::Thread* network_thread_;
};

}  // namespace webrtc

#endif  // VIDEO_RTP_VIDEO_STREAM_RECEIVER_DELEGATE_H_
