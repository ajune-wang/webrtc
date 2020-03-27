/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rtp_video_stream_receiver_frame_transformer_delegate.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread.h"
#include "video/rtp_video_stream_receiver.h"

namespace webrtc {

namespace {
class TransformableVideoReceiverFrame
    : public TransformableVideoFrameInterface {
 public:
  static std::unique_ptr<video_coding::RtpFrameObject> GetFrameObjectAndReset(
      std::unique_ptr<TransformableVideoReceiverFrame> transformable_frame) {
    auto frame = transformable_frame->PassFrame();
    transformable_frame.reset();
    return frame;
  }

  TransformableVideoReceiverFrame(
      std::unique_ptr<video_coding::RtpFrameObject> frame,
      uint32_t ssrc)
      : frame_(std::move(frame)), ssrc_(ssrc) {}
  ~TransformableVideoReceiverFrame() override = default;

  // Implements TransformableVideoFrameInterface.
  const uint8_t* GetData() const override {
    return frame_->GetEncodedData()->data();
  }

  void SetData(const uint8_t* data, size_t size) override {
    frame_->SetEncodedData(EncodedImageBuffer::Create(data, size));
  }

  size_t size() const override { return frame_->GetEncodedData()->size(); }
  uint32_t timestamp() const override { return frame_->Timestamp(); }
  uint32_t ssrc() const override { return ssrc_; }

  std::vector<uint8_t> additional_data() const override {
    return RtpDescriptorAuthentication(frame_->GetRtpVideoHeader());
  }

  const RTPVideoHeader& header() const override {
    return frame_->GetRtpVideoHeader();
  }

  bool is_keyframe() const override { return frame_->is_keyframe(); }

 private:
  std::unique_ptr<video_coding::RtpFrameObject> PassFrame() {
    return std::move(frame_);
  }

  std::unique_ptr<video_coding::RtpFrameObject> frame_;
  uint32_t ssrc_;
};
}  // namespace

RtpVideoStreamReceiverFrameTransformerDelegate::
    RtpVideoStreamReceiverFrameTransformerDelegate(
        RtpVideoStreamReceiver* receiver,
        rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
        rtc::Thread* network_thread)
    : receiver_(receiver),
      frame_transformer_(std::move(frame_transformer)),
      network_thread_(network_thread) {}

void RtpVideoStreamReceiverFrameTransformerDelegate::Init() {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  frame_transformer_->RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>(this));
}

void RtpVideoStreamReceiverFrameTransformerDelegate::Reset() {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  frame_transformer_->UnregisterTransformedFrameCallback();
  frame_transformer_ = nullptr;
  receiver_ = nullptr;
}

void RtpVideoStreamReceiverFrameTransformerDelegate::TransformFrame(
    std::unique_ptr<video_coding::RtpFrameObject> frame,
    uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  // TODO(bugs.webrtc.org/11380) remove once this version of TransformFrame is
  // deprecated.
  auto additional_data =
      RtpDescriptorAuthentication(frame->GetRtpVideoHeader());
  frame_transformer_->TransformFrame(std::move(frame),
                                     std::move(additional_data), ssrc);

  frame_transformer_->Transform(
      std::make_unique<TransformableVideoReceiverFrame>(std::move(frame),
                                                        ssrc));
}

void RtpVideoStreamReceiverFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate =
      this;
  network_thread_->PostTask(ToQueuedTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        delegate->ManageFrame(std::move(frame));
      }));
}

void RtpVideoStreamReceiverFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<TransformableFrameInterface> frame) {
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate =
      this;
  network_thread_->PostTask(ToQueuedTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        delegate->ManageFrame(std::move(frame));
      }));
}

void RtpVideoStreamReceiverFrameTransformerDelegate::ManageFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  if (!receiver_)
    return;
  auto transformed_frame = absl::WrapUnique(
      static_cast<video_coding::RtpFrameObject*>(frame.release()));
  receiver_->ManageFrame(std::move(transformed_frame));
}

void RtpVideoStreamReceiverFrameTransformerDelegate::ManageFrame(
    std::unique_ptr<TransformableFrameInterface> frame) {
  RTC_DCHECK_RUN_ON(&network_sequence_checker_);
  if (!receiver_)
    return;
  auto transformed_frame = absl::WrapUnique(
      static_cast<TransformableVideoReceiverFrame*>(frame.release()));
  receiver_->ManageFrame(
      TransformableVideoReceiverFrame::GetFrameObjectAndReset(
          std::move(transformed_frame)));
}

}  // namespace webrtc
