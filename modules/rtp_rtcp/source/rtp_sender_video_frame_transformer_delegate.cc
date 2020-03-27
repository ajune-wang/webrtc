/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {
namespace {

class TransformableVideoSenderFrame : public TransformableVideoFrameInterface {
 public:
  TransformableVideoSenderFrame(
      const EncodedImage& encoded_image,
      const RTPVideoHeader& video_header,
      int payload_type,
      absl::optional<VideoCodecType> codec_type,
      uint32_t rtp_timestamp,
      const RTPFragmentationHeader* fragmentation,
      absl::optional<int64_t> expected_retransmission_time_ms,
      uint32_t ssrc)
      : encoded_data_(encoded_image.GetEncodedData()),
        header_(video_header),
        frame_type_(encoded_image._frameType),
        payload_type_(payload_type),
        codec_type_(codec_type),
        timestamp_(rtp_timestamp),
        capture_time_ms_(encoded_image.capture_time_ms_),
        expected_retransmission_time_ms_(expected_retransmission_time_ms),
        ssrc_(ssrc) {
    if (fragmentation) {
      fragmentation_header_ = std::make_unique<RTPFragmentationHeader>();
      fragmentation_header_->CopyFrom(*fragmentation);
    }
  }

  ~TransformableVideoSenderFrame() override = default;

  // Implements TransformableVideoFrameInterface.
  uint8_t* GetData() const override { return encoded_data_->data(); }

  void SetData(const uint8_t* data, size_t size) override {
    encoded_data_ = EncodedImageBuffer::Create(data, size);
  }

  size_t size() const override { return encoded_data_->size(); }
  uint32_t timestamp() const override { return timestamp_; }
  uint32_t ssrc() const override { return ssrc_; }
  const RTPVideoHeader& header() const override { return header_; }

  bool is_keyframe() const override {
    return frame_type_ == VideoFrameType::kVideoFrameKey;
  }

  std::vector<uint8_t> additional_data() const override {
    return RtpDescriptorAuthentication(header_);
  }

  int payload_type() const { return payload_type_; }
  absl::optional<VideoCodecType> codec_type() const { return codec_type_; }
  int64_t capture_time_ms() const { return capture_time_ms_; }

  RTPFragmentationHeader* fragmentation_header() const {
    return fragmentation_header_.get();
  }

  const absl::optional<int64_t>& expected_retransmission_time_ms() const {
    return expected_retransmission_time_ms_;
  }

 private:
  rtc::scoped_refptr<EncodedImageBufferInterface> encoded_data_;
  RTPVideoHeader header_;
  VideoFrameType frame_type_;
  int payload_type_;
  absl::optional<VideoCodecType> codec_type_ = absl::nullopt;
  uint32_t timestamp_;
  int64_t capture_time_ms_;
  absl::optional<int64_t> expected_retransmission_time_ms_ = absl::nullopt;
  uint32_t ssrc_;
  std::unique_ptr<RTPFragmentationHeader> fragmentation_header_;
};
}  // namespace

RTPSenderVideoFrameTransformerDelegate::RTPSenderVideoFrameTransformerDelegate(
    RTPSenderVideo* sender,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
    : sender_(sender), frame_transformer_(std::move(frame_transformer)) {}

void RTPSenderVideoFrameTransformerDelegate::Init() {
  frame_transformer_->RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>(this));
}

bool RTPSenderVideoFrameTransformerDelegate::TransformFrame(
    int payload_type,
    absl::optional<VideoCodecType> codec_type,
    uint32_t rtp_timestamp,
    const EncodedImage& encoded_image,
    const RTPFragmentationHeader* fragmentation,
    RTPVideoHeader video_header,
    absl::optional<int64_t> expected_retransmission_time_ms,
    uint32_t ssrc) {
  if (!encoder_queue_)
    encoder_queue_ = TaskQueueBase::Current();
  frame_transformer_->Transform(std::make_unique<TransformableVideoSenderFrame>(
      encoded_image, video_header, payload_type, codec_type, rtp_timestamp,
      fragmentation, expected_retransmission_time_ms, ssrc));
  return true;
}

void RTPSenderVideoFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<TransformableFrameInterface> frame) {
  {
    rtc::CritScope lock(&sender_lock_);
    if (!sender_)
      return;
  }
  rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate> delegate = this;
  encoder_queue_->PostTask(ToQueuedTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        delegate->SendVideo(std::move(frame));
      }));
}

void RTPSenderVideoFrameTransformerDelegate::SendVideo(
    std::unique_ptr<TransformableFrameInterface> frame) const {
  RTC_CHECK(encoder_queue_->IsCurrent());
  rtc::CritScope lock(&sender_lock_);
  if (!sender_)
    return;
  auto* transformed_frame =
      static_cast<TransformableVideoSenderFrame*>(frame.get());
  sender_->SendVideo(
      transformed_frame->payload_type(), transformed_frame->codec_type(),
      transformed_frame->timestamp(), transformed_frame->capture_time_ms(),
      rtc::ArrayView<const uint8_t>(transformed_frame->GetData(),
                                    transformed_frame->size()),
      transformed_frame->fragmentation_header(), transformed_frame->header(),
      transformed_frame->expected_retransmission_time_ms());
}

void RTPSenderVideoFrameTransformerDelegate::SetVideoStructureUnderLock(
    const FrameDependencyStructure* video_structure) {
  rtc::CritScope lock(&sender_lock_);
  RTC_CHECK(sender_);
  sender_->SetVideoStructureUnderLock(video_structure);
}

void RTPSenderVideoFrameTransformerDelegate::Reset() {
  frame_transformer_->UnregisterTransformedFrameCallback();
  frame_transformer_ = nullptr;
  {
    rtc::CritScope lock(&sender_lock_);
    sender_ = nullptr;
  }
}
}  // namespace webrtc
