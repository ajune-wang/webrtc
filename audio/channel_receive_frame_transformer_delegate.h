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
#include "api/sequence_checker.h"
#include "rtc_base/buffer.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread.h"

namespace webrtc {

// Delegates calls to FrameTransformerInterface to transform frames, and to
// ChannelReceive to receive the transformed frames using the
// `receive_frame_callback_` on the `channel_receive_thread_`.
class ChannelReceiveFrameTransformerDelegate : public TransformedFrameCallback {
 public:
  using ReceiveFrameCallback =
      std::function<void(rtc::ArrayView<const uint8_t> packet,
                         const RTPHeader& header)>;
  ChannelReceiveFrameTransformerDelegate(
      ReceiveFrameCallback receive_frame_callback,
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
      TaskQueueBase* channel_receive_thread);

  // Registers `this` as callback for `frame_transformer_`, to get the
  // transformed frames.
  void Init();

  // Unregisters and releases the `frame_transformer_` reference, and resets
  // `receive_frame_callback_` on `channel_receive_thread_`. Called from
  // ChannelReceive destructor to prevent running the callback on a dangling
  // channel.
  void Reset();

  // Delegates the call to FrameTransformerInterface::Transform, to transform
  // the frame asynchronously.
  void Transform(rtc::ArrayView<const uint8_t> packet,
                 const RTPHeader& header,
                 uint32_t ssrc);

  // Implements TransformedFrameCallback. Can be called on any thread.
  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override;

  // Delegates the call to ChannelReceive::OnReceivedPayloadData on the
  // `channel_receive_thread_`, by calling `receive_frame_callback_`.
  void ReceiveFrame(std::unique_ptr<TransformableFrameInterface> frame) const;

 protected:
  ~ChannelReceiveFrameTransformerDelegate() override = default;

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  ReceiveFrameCallback receive_frame_callback_
      RTC_GUARDED_BY(sequence_checker_);
  rtc::scoped_refptr<FrameTransformerInterface> frame_transformer_
      RTC_GUARDED_BY(sequence_checker_);
  TaskQueueBase* const channel_receive_thread_;
};

class TransformableIncomingAudioFrame
    : public TransformableAudioFrameInterface {
 public:
  TransformableIncomingAudioFrame(rtc::ArrayView<const uint8_t> payload,
                                  const RTPHeader& header,
                                  uint32_t ssrc)
      : payload_(payload.data(), payload.size()),
        header_(header),
        ssrc_(ssrc) {}
  ~TransformableIncomingAudioFrame() override = default;
  rtc::ArrayView<const uint8_t> GetData() const override { return payload_; }

  void SetData(rtc::ArrayView<const uint8_t> data) override {
    payload_.SetData(data.data(), data.size());
  }

  void SetRTPTimestamp(uint32_t timestamp) override {
    header_.timestamp = timestamp;
  }

  uint8_t GetPayloadType() const override { return header_.payloadType; }
  uint32_t GetSsrc() const override { return ssrc_; }
  uint32_t GetTimestamp() const override { return header_.timestamp; }
  rtc::ArrayView<const uint32_t> GetContributingSources() const override {
    return rtc::ArrayView<const uint32_t>(header_.arrOfCSRCs, header_.numCSRCs);
  }
  Direction GetDirection() const override { return Direction::kReceiver; }

  const absl::optional<uint16_t> SequenceNumber() const override {
    return header_.sequenceNumber;
  }

  absl::optional<uint64_t> AbsoluteCaptureTimestamp() const override {
    // This could be extracted from received header extensions + extrapolation,
    // if required in future, eg for being able to re-send received frames.
    return absl::nullopt;
  }
  const RTPHeader& Header() const { return header_; }

  FrameType Type() const override {
    return header_.extension.voiceActivity ? FrameType::kAudioFrameSpeech
                                           : FrameType::kAudioFrameCN;
  }

 private:
  rtc::Buffer payload_;
  RTPHeader header_;
  uint32_t ssrc_;
};
}  // namespace webrtc
#endif  // AUDIO_CHANNEL_RECEIVE_FRAME_TRANSFORMER_DELEGATE_H_
