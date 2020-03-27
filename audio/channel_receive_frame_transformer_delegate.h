/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_CHANNEL_RECEIVE_FRAME_TRANSFORMER_DELEGATE_H_
#define AUDIO_CHANNEL_RECEIVE_FRAME_TRANSFORMER_DELEGATE_H_

#include <memory>

#include "api/frame_transformer_interface.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread.h"

namespace webrtc {
class ChannelReceiveFrameTransformerDelegate : public TransformedFrameCallback {
 public:
  using ReceiveFrameCallback = std::function<void(const uint8_t* packet,
                                                  size_t packet_length,
                                                  const RTPHeader& header)>;
  ChannelReceiveFrameTransformerDelegate(
      ReceiveFrameCallback receive_frame_callback,
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
      rtc::Thread* channel_receive_thread);

  void Init();
  void Reset();

  int32_t TransformFrame(const uint8_t* packet,
                         size_t packet_length,
                         const RTPHeader& header,
                         uint32_t ssrc);
  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override;

  void ReceiveFrame(std::unique_ptr<TransformableFrameInterface> frame) const;

 protected:
  ~ChannelReceiveFrameTransformerDelegate() override = default;

 private:
  SequenceChecker sequence_checker_;
  ReceiveFrameCallback receive_frame_callback_
      RTC_GUARDED_BY(sequence_checker_);
  rtc::scoped_refptr<FrameTransformerInterface> frame_transformer_
      RTC_GUARDED_BY(sequence_checker_);
  rtc::Thread* channel_receive_thread_;
};

}  // namespace webrtc
#endif  // AUDIO_CHANNEL_RECEIVE_FRAME_TRANSFORMER_DELEGATE_H_
