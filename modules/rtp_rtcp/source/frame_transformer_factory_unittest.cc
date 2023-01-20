/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/frame_transformer_factory.h"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/call/transport.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_frame_transformer.h"

namespace webrtc {
namespace {

using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

class MockTransformableVideoFrame
    : public webrtc::TransformableVideoFrameInterface {
 public:
  MOCK_METHOD(rtc::ArrayView<const uint8_t>, GetData, (), (const override));
  MOCK_METHOD(void, SetData, (rtc::ArrayView<const uint8_t> data), (override));
  MOCK_METHOD(uint8_t, GetPayloadType, (), (const, override));
  MOCK_METHOD(uint32_t, GetSsrc, (), (const, override));
  MOCK_METHOD(uint32_t, GetTimestamp, (), (const, override));
  MOCK_METHOD(TransformableFrameInterface::Direction,
              GetDirection,
              (),
              (const, override));
  MOCK_METHOD(bool, IsKeyFrame, (), (const, override));
  MOCK_METHOD(std::vector<uint8_t>, GetAdditionalData, (), (const, override));
  MOCK_METHOD(const webrtc::RTPVideoHeader&, GetHeader, (), (const, override));
  MOCK_METHOD(const webrtc::VideoFrameMetadata&,
              GetMetadata,
              (),
              (const, override));
  MOCK_METHOD(void,
              SetMetadata,
              (const webrtc::VideoFrameMetadata&),
              (override));
};

class MockTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>();
  }
};

template <typename T>
class FrameTransformerFactoryTest : public testing::Test {};
using HeaderTypes = ::testing::Types<RTPVideoHeaderVP8,
                                     RTPVideoHeaderVP9,
                                     RTPVideoHeaderH264,
                                     RTPVideoHeaderLegacyGeneric>;
TYPED_TEST_SUITE(FrameTransformerFactoryTest, HeaderTypes);

template <typename T>
struct RTPHeaderTraits {};

template <>
struct RTPHeaderTraits<RTPVideoHeaderVP8> {
  static const VideoCodecType codec = kVideoCodecVP8;
  static RTPVideoHeaderCodecSpecifics GetSpecifics() {
    RTPVideoHeaderVP8 specifics;
    specifics.InitRTPVideoHeaderVP8();
    return specifics;
  }
  static void SetSpecifics(VideoFrameMetadata& metadata) {
    RTPVideoHeaderVP8 specifics;
    specifics.InitRTPVideoHeaderVP8();
    metadata.SetRTPVideoHeaderCodecSpecifics(specifics);
  }
};

template <>
struct RTPHeaderTraits<RTPVideoHeaderVP9> {
  static const VideoCodecType codec = kVideoCodecVP9;
  static RTPVideoHeaderCodecSpecifics GetSpecifics() {
    RTPVideoHeaderVP9 specifics;
    specifics.InitRTPVideoHeaderVP9();
    return specifics;
  }
  static void SetSpecifics(VideoFrameMetadata& metadata) {
    RTPVideoHeaderVP9 specifics;
    specifics.InitRTPVideoHeaderVP9();
    metadata.SetRTPVideoHeaderCodecSpecifics(specifics);
  }
};

template <>
struct RTPHeaderTraits<RTPVideoHeaderH264> {
  static const VideoCodecType codec = kVideoCodecH264;
  static RTPVideoHeaderCodecSpecifics GetSpecifics() {
    return RTPVideoHeaderH264();
  }
  static void SetSpecifics(VideoFrameMetadata& metadata) {
    metadata.SetRTPVideoHeaderCodecSpecifics(RTPVideoHeaderH264());
  }
};

template <>
struct RTPHeaderTraits<RTPVideoHeaderLegacyGeneric> {
  static const VideoCodecType codec = kVideoCodecGeneric;
  static RTPVideoHeaderCodecSpecifics GetSpecifics() {
    return absl::monostate();
  }
  static void SetSpecifics(VideoFrameMetadata& metadata) {}
};

TYPED_TEST(FrameTransformerFactoryTest, CloneVideoFrame) {
  NiceMock<MockTransformableVideoFrame> original_frame;
  uint8_t data[10];
  std::fill_n(data, 10, 5);
  rtc::ArrayView<uint8_t> data_view(data);
  EXPECT_CALL(original_frame, GetData()).WillRepeatedly(Return(data_view));

  VideoFrameMetadata original_metadata;
  original_metadata.SetFrameType(VideoFrameType::kVideoFrameKey);
  original_metadata.SetWidth(640);
  original_metadata.SetHeight(480);
  original_metadata.SetRotation(VideoRotation::kVideoRotation_90);
  original_metadata.SetContentType(VideoContentType::SCREENSHARE);
  original_metadata.SetFrameId(17);
  original_metadata.SetSpatialIndex(23);
  original_metadata.SetTemporalIndex(37);
  std::vector<int64_t> frame_dependencies{int64_t(13)};
  original_metadata.SetFrameDependencies(frame_dependencies);
  std::vector<const DecodeTargetIndication> decode_target_indications{
      DecodeTargetIndication::kRequired};
  original_metadata.SetDecodeTargetIndications(decode_target_indications);
  original_metadata.SetIsLastFrameInPicture(true);
  original_metadata.SetSimulcastIdx(42);
  original_metadata.SetCodec(RTPHeaderTraits<TypeParam>::codec);
  auto specifics = RTPHeaderTraits<TypeParam>::GetSpecifics();
  if (specifics.index() != 0) {
    original_metadata.SetRTPVideoHeaderCodecSpecifics(specifics);
  }
  EXPECT_CALL(original_frame, GetMetadata())
      .WillRepeatedly(ReturnRef(original_metadata));

  auto cloned_frame = CloneVideoFrame(&original_frame);
  VideoFrameMetadata cloned_metadata = cloned_frame->GetMetadata();

  EXPECT_EQ(cloned_frame->GetData().size(), 10u);
  EXPECT_THAT(cloned_frame->GetData(), testing::Each(5u));
  EXPECT_EQ(cloned_metadata.GetFrameType(), VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(cloned_metadata.GetWidth(), 640);
  EXPECT_EQ(cloned_metadata.GetHeight(), 480);
  EXPECT_EQ(cloned_metadata.GetRotation(), VideoRotation::kVideoRotation_90);
  EXPECT_EQ(cloned_metadata.GetContentType(), VideoContentType::SCREENSHARE);
  EXPECT_EQ(cloned_metadata.GetFrameId(), 17);
  EXPECT_EQ(cloned_metadata.GetSpatialIndex(), 23);
  EXPECT_EQ(cloned_metadata.GetTemporalIndex(), 37);
  EXPECT_EQ(cloned_metadata.GetFrameDependencies()[0], 13);
  EXPECT_EQ(cloned_metadata.GetDecodeTargetIndications()[0],
            DecodeTargetIndication::kRequired);
  EXPECT_EQ(cloned_metadata.GetIsLastFrameInPicture(), true);
  EXPECT_EQ(cloned_metadata.GetSimulcastIdx(), 42);
  EXPECT_EQ(cloned_metadata.GetCodec(),
            VideoCodecType(RTPHeaderTraits<TypeParam>::codec));
  EXPECT_EQ(RTPHeaderTraits<TypeParam>::GetSpecifics().index(),
            cloned_metadata.GetRTPVideoHeaderCodecSpecifics().index());

  auto cloned_header = cloned_frame->GetHeader();
  EXPECT_EQ(cloned_header.codec,
            VideoCodecType(RTPHeaderTraits<TypeParam>::codec));
  EXPECT_EQ(RTPHeaderTraits<TypeParam>::GetSpecifics().index(),
            cloned_header.video_type_header.index());
}

}  // namespace
}  // namespace webrtc
