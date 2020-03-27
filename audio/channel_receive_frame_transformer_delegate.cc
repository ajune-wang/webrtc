/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive_frame_transformer_delegate.h"

#include <utility>

#include "rtc_base/buffer.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

namespace {
class TransformableAudioFrame : public TransformableAudioFrameInterface {
 public:
  TransformableAudioFrame(const uint8_t* payload,
                          size_t payload_size,
                          const RTPHeader& header,
                          uint32_t ssrc)
      : payload_(payload, payload_size), header_(header), ssrc_(ssrc) {}
  ~TransformableAudioFrame() override = default;
  const uint8_t* GetData() const override { return payload_.data(); }
  void SetData(const uint8_t* data, size_t size) override {
    payload_.SetData(data, size);
  }
  size_t size() const override { return payload_.size(); }
  uint32_t timestamp() const override { return header_.timestamp; }
  uint32_t ssrc() const override { return ssrc_; }
  const RTPHeader& header() const override { return header_; }
  const uint8_t* packet() const { return payload_.data(); }
  size_t packet_length() const { return payload_.size(); }

 private:
  rtc::Buffer payload_;
  RTPHeader header_;
  uint32_t ssrc_;
};
}  // namespace

ChannelReceiveFrameTransformerDelegate::ChannelReceiveFrameTransformerDelegate(
    ReceiveFrameCallback receive_frame_callback,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
    rtc::Thread* channel_receive_thread)
    : receive_frame_callback_(receive_frame_callback),
      frame_transformer_(std::move(frame_transformer)),
      channel_receive_thread_(channel_receive_thread) {}

void ChannelReceiveFrameTransformerDelegate::Init() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  frame_transformer_->RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>(this));
}

void ChannelReceiveFrameTransformerDelegate::Reset() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  frame_transformer_->UnregisterTransformedFrameCallback();
  frame_transformer_ = nullptr;
  receive_frame_callback_ = ReceiveFrameCallback();
}

int32_t ChannelReceiveFrameTransformerDelegate::TransformFrame(
    const uint8_t* packet,
    size_t packet_length,
    const RTPHeader& header,
    uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  frame_transformer_->Transform(std::make_unique<TransformableAudioFrame>(
      packet, packet_length, header, ssrc));
  return 0;
}

void ChannelReceiveFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<TransformableFrameInterface> frame) {
  rtc::scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate = this;
  channel_receive_thread_->PostTask(ToQueuedTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        delegate->ReceiveFrame(std::move(frame));
      }));
}

void ChannelReceiveFrameTransformerDelegate::ReceiveFrame(
    std::unique_ptr<TransformableFrameInterface> frame) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!receive_frame_callback_)
    return;
  auto* transformed_frame = static_cast<TransformableAudioFrame*>(frame.get());
  receive_frame_callback_(transformed_frame->packet(),
                          transformed_frame->packet_length(),
                          transformed_frame->header());
}
}  // namespace webrtc
