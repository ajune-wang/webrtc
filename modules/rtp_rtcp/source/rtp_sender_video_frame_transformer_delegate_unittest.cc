/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

#include <string>
#include <utility>

#include "api/test/mock_frame_transformer.h"
#include "api/test/mock_transformable_video_frame.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithoutArgs;

class MockRTPVideoFrameSenderInterface : public RTPVideoFrameSenderInterface {
 public:
  MOCK_METHOD(bool,
              SendVideo,
              (int payload_type,
               absl::optional<VideoCodecType> codec_type,
               uint32_t rtp_timestamp,
               Timestamp capture_time,
               rtc::ArrayView<const uint8_t> payload,
               size_t encoder_output_size,
               RTPVideoHeader video_header,
               TimeDelta expected_retransmission_time,
               std::vector<uint32_t> csrcs),
              (override));

  MOCK_METHOD(void,
              SetVideoStructureAfterTransformation,
              (const FrameDependencyStructure* video_structure),
              (override));
  MOCK_METHOD(void,
              SetVideoLayersAllocationAfterTransformation,
              (VideoLayersAllocation allocation),
              (override));
};

class MockTransformableVideoFrameWithDependencies
    : public TransformableVideoFrameWithDependencies {
 public:
  MOCK_METHOD(rtc::ArrayView<const uint8_t>, GetData, (), (const, override));
  MOCK_METHOD(void, SetData, (rtc::ArrayView<const uint8_t> data), (override));
  MOCK_METHOD(uint32_t, GetTimestamp, (), (const, override));
  MOCK_METHOD(void, SetRTPTimestamp, (uint32_t), (override));
  MOCK_METHOD(uint32_t, GetSsrc, (), (const, override));
  MOCK_METHOD(bool, IsKeyFrame, (), (const, override));
  MOCK_METHOD(void,
              SetMetadata,
              (const webrtc::VideoFrameMetadata&),
              (override));
  MOCK_METHOD(uint8_t, GetPayloadType, (), (const, override));
  MOCK_METHOD(TransformableFrameInterface::Direction,
              GetDirection,
              (),
              (const, override));
  MOCK_METHOD(std::string, GetMimeType, (), (const, override));
  MOCK_METHOD(VideoFrameMetadata, Metadata, (), (const, override));
  MOCK_METHOD(absl::optional<Timestamp>,
              GetCaptureTimeIdentifier,
              (),
              (const, override));
  MOCK_METHOD(const FrameDependencyStructure*,
              GetFrameDependencyStructure,
              (),
              (const, override));
};

class RtpSenderVideoFrameTransformerDelegateTest : public ::testing::Test {
 protected:
  RtpSenderVideoFrameTransformerDelegateTest()
      : frame_transformer_(rtc::make_ref_counted<MockFrameTransformer>()),
        time_controller_(Timestamp::Seconds(0)) {}

  ~RtpSenderVideoFrameTransformerDelegateTest() override = default;

  std::unique_ptr<TransformableFrameInterface> GetTransformableFrame(
      rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate> delegate,
      bool key_frame = false) {
    EncodedImage encoded_image;
    encoded_image.SetEncodedData(EncodedImageBuffer::Create(1));
    encoded_image._frameType = key_frame ? VideoFrameType::kVideoFrameKey
                                         : VideoFrameType::kVideoFrameDelta;
    std::unique_ptr<TransformableFrameInterface> frame = nullptr;
    EXPECT_CALL(*frame_transformer_, Transform)
        .WillOnce([&](std::unique_ptr<TransformableFrameInterface>
                          frame_to_transform) {
          frame = std::move(frame_to_transform);
        });
    RTPVideoHeader rtp_header;

    VideoFrameMetadata metadata;
    metadata.SetCodec(VideoCodecType::kVideoCodecVP8);
    metadata.SetRTPVideoHeaderCodecSpecifics(RTPVideoHeaderVP8());

    delegate->TransformFrame(
        /*payload_type=*/1, VideoCodecType::kVideoCodecVP8, /*rtp_timestamp=*/2,
        encoded_image, RTPVideoHeader::FromMetadata(metadata),
        /*expected_retransmission_time=*/TimeDelta::Millis(10),
        /*frame_dependency_structure_getter=*/[]() { return nullptr; });
    return frame;
  }

  MockRTPVideoFrameSenderInterface test_sender_;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer_;
  GlobalSimulatedTimeController time_controller_;
};

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       RegisterTransformedFrameCallbackSinkOnInit) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());
  EXPECT_CALL(*frame_transformer_,
              RegisterTransformedFrameSinkCallback(_, 1111));
  delegate->Init();
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       UnregisterTransformedFrameSinkCallbackOnReset) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());
  EXPECT_CALL(*frame_transformer_,
              UnregisterTransformedFrameSinkCallback(1111));
  delegate->Reset();
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       TransformFrameCallsTransform) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());

  EncodedImage encoded_image;
  EXPECT_CALL(*frame_transformer_, Transform);
  delegate->TransformFrame(
      /*payload_type=*/1, VideoCodecType::kVideoCodecVP8, /*rtp_timestamp=*/2,
      encoded_image, RTPVideoHeader(),
      /*expected_retransmission_time=*/TimeDelta::Millis(10),
      /*frame_dependency_structure_getter=*/[]() { return nullptr; });
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       OnTransformedFrameCallsSenderSendVideo) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());

  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*frame_transformer_, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);
  EXPECT_STRCASEEQ("video/VP8", frame->GetMimeType().c_str());

  rtc::Event event;
  EXPECT_CALL(test_sender_, SendVideo).WillOnce(WithoutArgs([&] {
    event.Set();
    return true;
  }));

  callback->OnTransformedFrame(std::move(frame));

  event.Wait(TimeDelta::Seconds(1));
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, CloneSenderVideoFrame) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);

  auto& video_frame = static_cast<TransformableVideoFrameInterface&>(*frame);
  std::unique_ptr<TransformableVideoFrameInterface> clone =
      CloneSenderVideoFrame(&video_frame);

  EXPECT_EQ(clone->IsKeyFrame(), video_frame.IsKeyFrame());
  EXPECT_EQ(clone->GetPayloadType(), video_frame.GetPayloadType());
  EXPECT_EQ(clone->GetMimeType(), video_frame.GetMimeType());
  EXPECT_EQ(clone->GetSsrc(), video_frame.GetSsrc());
  EXPECT_EQ(clone->GetTimestamp(), video_frame.GetTimestamp());
  EXPECT_EQ(clone->Metadata(), video_frame.Metadata());
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, CloneKeyFrame) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate, /*key_frame=*/true);
  ASSERT_TRUE(frame);

  auto& video_frame = static_cast<TransformableVideoFrameInterface&>(*frame);
  std::unique_ptr<TransformableVideoFrameInterface> clone =
      CloneSenderVideoFrame(&video_frame);

  EXPECT_EQ(clone->IsKeyFrame(), video_frame.IsKeyFrame());
  EXPECT_EQ(clone->GetPayloadType(), video_frame.GetPayloadType());
  EXPECT_EQ(clone->GetMimeType(), video_frame.GetMimeType());
  EXPECT_EQ(clone->GetSsrc(), video_frame.GetSsrc());
  EXPECT_EQ(clone->GetTimestamp(), video_frame.GetTimestamp());
  EXPECT_EQ(clone->Metadata(), video_frame.Metadata());
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, CloneReceivedVideoFrame) {
  const uint8_t payload_type = 1;
  const uint32_t timestamp = 2;
  const std::vector<uint32_t> frame_csrcs = {123, 456, 789};

  auto mock_receiver_frame =
      std::make_unique<NiceMock<MockTransformableVideoFrameWithDependencies>>();
  ON_CALL(*mock_receiver_frame, GetDirection)
      .WillByDefault(Return(TransformableFrameInterface::Direction::kReceiver));
  VideoFrameMetadata metadata;
  metadata.SetCodec(kVideoCodecVP8);
  metadata.SetRTPVideoHeaderCodecSpecifics(RTPVideoHeaderVP8());
  metadata.SetCsrcs(frame_csrcs);
  ON_CALL(*mock_receiver_frame, Metadata).WillByDefault(Return(metadata));
  rtc::scoped_refptr<EncodedImageBufferInterface> buffer =
      EncodedImageBuffer::Create(1);
  ON_CALL(*mock_receiver_frame, GetData)
      .WillByDefault(Return(rtc::ArrayView<const uint8_t>(*buffer)));
  ON_CALL(*mock_receiver_frame, GetPayloadType)
      .WillByDefault(Return(payload_type));
  ON_CALL(*mock_receiver_frame, GetTimestamp).WillByDefault(Return(timestamp));
  ON_CALL(*mock_receiver_frame, GetMimeType).WillByDefault(Return("video/VP8"));
  FrameDependencyStructure dependency_structure;
  dependency_structure.num_decode_targets = 1;
  dependency_structure.num_chains = 1;
  dependency_structure.decode_target_protected_by_chain.push_back(0);
  ON_CALL(*mock_receiver_frame, GetFrameDependencyStructure)
      .WillByDefault(Return(&dependency_structure));

  std::unique_ptr<TransformableVideoFrameInterface> clone_base =
      CloneSenderVideoFrame(mock_receiver_frame.get());
  TransformableVideoFrameWithDependencies* clone =
      static_cast<TransformableVideoFrameWithDependencies*>(clone_base.get());

  EXPECT_EQ(clone->IsKeyFrame(), mock_receiver_frame->IsKeyFrame());
  EXPECT_EQ(clone->GetPayloadType(), mock_receiver_frame->GetPayloadType());
  EXPECT_EQ(clone->GetMimeType(), mock_receiver_frame->GetMimeType());
  EXPECT_EQ(clone->GetSsrc(), mock_receiver_frame->GetSsrc());
  EXPECT_EQ(clone->GetTimestamp(), mock_receiver_frame->GetTimestamp());
  EXPECT_EQ(clone->Metadata(), mock_receiver_frame->Metadata());

  // Clone should have a new but equal instance of FrameDependencyStructure.
  EXPECT_NE(clone->GetFrameDependencyStructure(),
            mock_receiver_frame->GetFrameDependencyStructure());
  EXPECT_EQ(*clone->GetFrameDependencyStructure(),
            *mock_receiver_frame->GetFrameDependencyStructure());
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, MetadataAfterSetMetadata) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);
  auto& video_frame = static_cast<TransformableVideoFrameInterface&>(*frame);

  VideoFrameMetadata metadata;
  metadata.SetFrameType(VideoFrameType::kVideoFrameKey);
  metadata.SetFrameId(654);
  metadata.SetSsrc(2222);
  metadata.SetCsrcs({1, 2, 3});

  video_frame.SetMetadata(metadata);
  VideoFrameMetadata actual_metadata = video_frame.Metadata();

  // TODO(bugs.webrtc.org/14708): Just EXPECT_EQ the whole Metadata once the
  // equality operator lands.
  EXPECT_EQ(metadata.GetFrameType(), actual_metadata.GetFrameType());
  EXPECT_EQ(metadata.GetFrameId(), actual_metadata.GetFrameId());
  EXPECT_EQ(metadata.GetSsrc(), actual_metadata.GetSsrc());
  EXPECT_EQ(metadata.GetCsrcs(), actual_metadata.GetCsrcs());
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       ReceiverFrameConvertedToSenderFrame) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());

  const uint8_t payload_type = 1;
  const uint32_t timestamp = 2;
  const std::vector<uint32_t> frame_csrcs = {123, 456, 789};

  auto mock_receiver_frame =
      std::make_unique<NiceMock<MockTransformableVideoFrameWithDependencies>>();
  ON_CALL(*mock_receiver_frame, GetDirection)
      .WillByDefault(Return(TransformableFrameInterface::Direction::kReceiver));
  VideoFrameMetadata metadata;
  metadata.SetCodec(kVideoCodecVP8);
  metadata.SetRTPVideoHeaderCodecSpecifics(RTPVideoHeaderVP8());
  metadata.SetCsrcs(frame_csrcs);
  ON_CALL(*mock_receiver_frame, Metadata).WillByDefault(Return(metadata));
  rtc::ArrayView<const uint8_t> buffer =
      (rtc::ArrayView<const uint8_t>)*EncodedImageBuffer::Create(1);
  ON_CALL(*mock_receiver_frame, GetData).WillByDefault(Return(buffer));
  ON_CALL(*mock_receiver_frame, GetPayloadType)
      .WillByDefault(Return(payload_type));
  ON_CALL(*mock_receiver_frame, GetTimestamp).WillByDefault(Return(timestamp));

  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*frame_transformer_, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  rtc::Event event;
  EXPECT_CALL(
      test_sender_,
      SendVideo(payload_type, absl::make_optional(kVideoCodecVP8), timestamp,
                /*capture_time=*/Timestamp::MinusInfinity(), buffer, _, _,
                /*expected_retransmission_time=*/TimeDelta::Millis(10),
                frame_csrcs))
      .WillOnce(WithoutArgs([&] {
        event.Set();
        return true;
      }));

  callback->OnTransformedFrame(std::move(mock_receiver_frame));

  event.Wait(TimeDelta::Seconds(1));
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       ReceiverFrameWithVideoStructureConvertedToSenderFrame) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());

  const uint8_t payload_type = 1;
  const uint32_t timestamp = 2;
  const std::vector<uint32_t> frame_csrcs = {123, 456, 789};

  auto mock_receiver_frame =
      std::make_unique<NiceMock<MockTransformableVideoFrameWithDependencies>>();
  ON_CALL(*mock_receiver_frame, GetDirection)
      .WillByDefault(Return(TransformableFrameInterface::Direction::kReceiver));
  VideoFrameMetadata metadata;
  metadata.SetCodec(kVideoCodecVP8);
  metadata.SetRTPVideoHeaderCodecSpecifics(RTPVideoHeaderVP8());
  metadata.SetCsrcs(frame_csrcs);
  ON_CALL(*mock_receiver_frame, Metadata).WillByDefault(Return(metadata));
  rtc::ArrayView<const uint8_t> buffer =
      (rtc::ArrayView<const uint8_t>)*EncodedImageBuffer::Create(1);
  ON_CALL(*mock_receiver_frame, GetData).WillByDefault(Return(buffer));
  ON_CALL(*mock_receiver_frame, GetPayloadType)
      .WillByDefault(Return(payload_type));
  ON_CALL(*mock_receiver_frame, GetTimestamp).WillByDefault(Return(timestamp));
  FrameDependencyStructure dependency_structure;
  dependency_structure.num_decode_targets = 1;
  dependency_structure.num_chains = 1;
  dependency_structure.decode_target_protected_by_chain.push_back(0);
  ON_CALL(*mock_receiver_frame, GetFrameDependencyStructure)
      .WillByDefault(Return(&dependency_structure));

  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*frame_transformer_, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  rtc::Event event;
  EXPECT_CALL(test_sender_,
              SetVideoStructureAfterTransformation(&dependency_structure));
  EXPECT_CALL(
      test_sender_,
      SendVideo(payload_type, absl::make_optional(kVideoCodecVP8), timestamp,
                /*capture_time=*/Timestamp::MinusInfinity(), buffer, _, _,
                /*expected_retransmission_time=*/TimeDelta::Millis(10),
                frame_csrcs))
      .WillOnce(WithoutArgs([&] {
        event.Set();
        return true;
      }));

  callback->OnTransformedFrame(std::move(mock_receiver_frame));

  event.Wait(TimeDelta::Seconds(1));
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, SettingRTPTimestamp) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);
  auto& video_frame = static_cast<TransformableVideoFrameInterface&>(*frame);

  uint32_t rtp_timestamp = 12345;
  ASSERT_FALSE(video_frame.GetTimestamp() == rtp_timestamp);

  video_frame.SetRTPTimestamp(rtp_timestamp);
  EXPECT_EQ(video_frame.GetTimestamp(), rtp_timestamp);
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       ShortCircuitingSkipsTransform) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, time_controller_.CreateTaskQueueFactory().get());
  EXPECT_CALL(*frame_transformer_,
              RegisterTransformedFrameSinkCallback(_, 1111));
  delegate->Init();

  delegate->StartShortCircuiting();

  // Will not call the actual transformer.
  EXPECT_CALL(*frame_transformer_, Transform).Times(0);
  // Will pass the frame straight to the reciever.
  EXPECT_CALL(test_sender_, SendVideo);

  EncodedImage encoded_image;
  encoded_image.SetEncodedData(EncodedImageBuffer::Create(1));
  delegate->TransformFrame(
      /*payload_type=*/1, VideoCodecType::kVideoCodecVP8, /*rtp_timestamp=*/2,
      encoded_image, RTPVideoHeader(),
      /*expected_retransmission_time=*/TimeDelta::Millis(10),
      /*frame_dependency_structure_getter=*/[]() { return nullptr; });
}

}  // namespace
}  // namespace webrtc
