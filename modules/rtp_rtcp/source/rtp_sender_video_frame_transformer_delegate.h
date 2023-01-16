/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_FRAME_TRANSFORMER_DELEGATE_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_FRAME_TRANSFORMER_DELEGATE_H_

#include <memory>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video/video_layers_allocation.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
class TransformableVideoSenderFrame : public TransformableVideoFrameInterface {
 public:
  TransformableVideoSenderFrame(
      const EncodedImage& encoded_image,
      const RTPVideoHeader& video_header,
      int payload_type,
      absl::optional<VideoCodecType> codec_type,
      uint32_t rtp_timestamp,
      absl::optional<int64_t> expected_retransmission_time_ms,
      uint32_t ssrc);

  ~TransformableVideoSenderFrame() override = default;

  // Implements TransformableVideoFrameInterface.
  rtc::ArrayView<const uint8_t> GetData() const override;

  void SetData(rtc::ArrayView<const uint8_t> data) override;

  uint32_t GetTimestamp() const override;
  uint32_t GetSsrc() const override;

  bool IsKeyFrame() const override;

  std::vector<uint8_t> GetAdditionalData() const override;

  const VideoFrameMetadata& GetMetadata() const override;

  const RTPVideoHeader& GetHeader() const;
  uint8_t GetPayloadType() const override;
  absl::optional<VideoCodecType> GetCodecType() const;
  int64_t GetCaptureTimeMs() const;

  const absl::optional<int64_t>& GetExpectedRetransmissionTimeMs() const;

  Direction GetDirection() const override;

 private:
  rtc::scoped_refptr<EncodedImageBufferInterface> encoded_data_;
  RTPVideoHeader header_;
  // This is a copy of `header_.GetAsMetadata()`, only needed because the
  // interface says GetMetadata() must return a const ref rather than a value.
  // TODO(crbug.com/webrtc/14709): Change the interface and delete this variable
  // to reduce risk of it getting out-of-sync with `header_.GetAsMetadata()`.
  VideoFrameMetadata metadata_;
  const VideoFrameType frame_type_;
  const uint8_t payload_type_;
  const absl::optional<VideoCodecType> codec_type_ = absl::nullopt;
  const uint32_t timestamp_;
  const int64_t capture_time_ms_;
  const absl::optional<int64_t> expected_retransmission_time_ms_;
  const uint32_t ssrc_;
};

class RTPSenderVideo;

// Delegates calls to FrameTransformerInterface to transform frames, and to
// RTPSenderVideo to send the transformed frames. Ensures thread-safe access to
// the sender.
class RTPSenderVideoFrameTransformerDelegate : public TransformedFrameCallback {
 public:
  RTPSenderVideoFrameTransformerDelegate(
      RTPSenderVideo* sender,
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
      uint32_t ssrc,
      TaskQueueFactory* send_transport_queue);

  void Init();

  // Delegates the call to FrameTransformerInterface::TransformFrame.
  bool TransformFrame(int payload_type,
                      absl::optional<VideoCodecType> codec_type,
                      uint32_t rtp_timestamp,
                      const EncodedImage& encoded_image,
                      RTPVideoHeader video_header,
                      absl::optional<int64_t> expected_retransmission_time_ms);

  // Implements TransformedFrameCallback. Can be called on any thread. Posts
  // the transformed frame to be sent on the `encoder_queue_`.
  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override;

  // Delegates the call to RTPSendVideo::SendVideo on the `encoder_queue_`.
  void SendVideo(std::unique_ptr<TransformableFrameInterface> frame) const
      RTC_RUN_ON(transformation_queue_);

  // Delegates the call to RTPSendVideo::SetVideoStructureAfterTransformation
  // under `sender_lock_`.
  void SetVideoStructureUnderLock(
      const FrameDependencyStructure* video_structure);

  // Delegates the call to
  // RTPSendVideo::SetVideoLayersAllocationAfterTransformation under
  // `sender_lock_`.
  void SetVideoLayersAllocationUnderLock(VideoLayersAllocation allocation);

  // Unregisters and releases the `frame_transformer_` reference, and resets
  // `sender_` under lock. Called from RTPSenderVideo destructor to prevent the
  // `sender_` to dangle.
  void Reset();

 protected:
  ~RTPSenderVideoFrameTransformerDelegate() override = default;

 private:
  void EnsureEncoderQueueCreated();

  mutable Mutex sender_lock_;
  RTPSenderVideo* sender_ RTC_GUARDED_BY(sender_lock_);
  rtc::scoped_refptr<FrameTransformerInterface> frame_transformer_;
  const uint32_t ssrc_;
  // Used when the encoded frames arrives without a current task queue. This can
  // happen if a hardware encoder was used.
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> transformation_queue_;
};

// Method to support cloning a Sender frame from another frame
std::unique_ptr<TransformableVideoFrameInterface> CloneSenderVideoFrame(
    TransformableVideoFrameInterface* original);

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_FRAME_TRANSFORMER_DELEGATE_H_
