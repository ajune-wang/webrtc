/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <set>

#include "absl/types/optional.h"
#include "call/rtp_payload_params.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const uint32_t kSsrc1 = 12345;
const uint32_t kSsrc2 = 23456;
const int16_t kPictureId = 123;
const int16_t kTl0PicIdx = 20;
const uint8_t kTemporalIdx = 1;
const int16_t kInitialPictureId1 = 222;
const int16_t kInitialTl0PicIdx1 = 99;
const int64_t kDontCare = 0;
}  // namespace

TEST(RtpPayloadParamsTest, InfoMappedToRtpVideoHeader_Vp8) {
  RtpPayloadState state2;
  state2.picture_id = kPictureId;
  state2.tl0_pic_idx = kTl0PicIdx;
  std::map<uint32_t, RtpPayloadState> states = {{kSsrc2, state2}};

  RtpPayloadParams params(kSsrc2, &state2);
  EncodedImage encoded_image;
  encoded_image.rotation_ = kVideoRotation_90;
  encoded_image.content_type_ = VideoContentType::SCREENSHARE;
  encoded_image.SetSpatialIndex(1);

  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = 0;
  codec_info.codecSpecific.VP8.keyIdx = kNoKeyIdx;
  codec_info.codecSpecific.VP8.layerSync = false;
  codec_info.codecSpecific.VP8.nonReference = true;

  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = 1;
  codec_info.codecSpecific.VP8.layerSync = true;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, 1);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(1, header.simulcastIdx);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(header.video_type_header);
  EXPECT_EQ(kPictureId + 2, vp8_header.pictureId);
  EXPECT_EQ(kTemporalIdx, vp8_header.temporalIdx);
  EXPECT_EQ(kTl0PicIdx + 1, vp8_header.tl0PicIdx);
  EXPECT_EQ(kNoKeyIdx, vp8_header.keyIdx);
  EXPECT_TRUE(vp8_header.layerSync);
  EXPECT_TRUE(vp8_header.nonReference);
}

TEST(RtpPayloadParamsTest, InfoMappedToRtpVideoHeader_Vp9) {
  RtpPayloadState state;
  state.picture_id = kPictureId;
  state.tl0_pic_idx = kTl0PicIdx;
  RtpPayloadParams params(kSsrc1, &state);

  EncodedImage encoded_image;
  encoded_image.rotation_ = kVideoRotation_90;
  encoded_image.content_type_ = VideoContentType::SCREENSHARE;
  encoded_image.SetSpatialIndex(0);
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.num_spatial_layers = 3;
  codec_info.codecSpecific.VP9.first_frame_in_picture = true;
  codec_info.codecSpecific.VP9.temporal_idx = 2;
  codec_info.codecSpecific.VP9.end_of_picture = false;

  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(kVideoCodecVP9, header.codec);
  const auto& vp9_header =
      absl::get<RTPVideoHeaderVP9>(header.video_type_header);
  EXPECT_EQ(kPictureId + 1, vp9_header.picture_id);
  EXPECT_EQ(kTl0PicIdx, vp9_header.tl0_pic_idx);
  EXPECT_EQ(vp9_header.temporal_idx, codec_info.codecSpecific.VP9.temporal_idx);
  EXPECT_EQ(vp9_header.spatial_idx, encoded_image.SpatialIndex());
  EXPECT_EQ(vp9_header.num_spatial_layers,
            codec_info.codecSpecific.VP9.num_spatial_layers);
  EXPECT_EQ(vp9_header.end_of_picture,
            codec_info.codecSpecific.VP9.end_of_picture);

  // Next spatial layer.
  codec_info.codecSpecific.VP9.first_frame_in_picture = false;
  codec_info.codecSpecific.VP9.end_of_picture = true;

  encoded_image.SetSpatialIndex(1);
  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoRotation_90, header.rotation);
  EXPECT_EQ(VideoContentType::SCREENSHARE, header.content_type);
  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kPictureId + 1, vp9_header.picture_id);
  EXPECT_EQ(kTl0PicIdx, vp9_header.tl0_pic_idx);
  EXPECT_EQ(vp9_header.temporal_idx, codec_info.codecSpecific.VP9.temporal_idx);
  EXPECT_EQ(vp9_header.spatial_idx, encoded_image.SpatialIndex());
  EXPECT_EQ(vp9_header.num_spatial_layers,
            codec_info.codecSpecific.VP9.num_spatial_layers);
  EXPECT_EQ(vp9_header.end_of_picture,
            codec_info.codecSpecific.VP9.end_of_picture);
}

TEST(RtpPayloadParamsTest, InfoMappedToRtpVideoHeader_H264) {
  RtpPayloadParams params(kSsrc1, {});

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecH264;
  codec_info.codecSpecific.H264.packetization_mode =
      H264PacketizationMode::SingleNalUnit;

  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(0, header.simulcastIdx);
  EXPECT_EQ(kVideoCodecH264, header.codec);
  const auto& h264 = absl::get<RTPVideoHeaderH264>(header.video_type_header);
  EXPECT_EQ(H264PacketizationMode::SingleNalUnit, h264.packetization_mode);
}

TEST(RtpPayloadParamsTest, PictureIdIsSetForVp8) {
  RtpPayloadState state;
  state.picture_id = kInitialPictureId1;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;

  RtpPayloadParams params(kSsrc1, &state);
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 1,
            absl::get<RTPVideoHeaderVP8>(header.video_type_header).pictureId);

  // State should hold latest used picture id and tl0_pic_idx.
  state = params.state();
  EXPECT_EQ(kInitialPictureId1 + 1, state.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, state.tl0_pic_idx);
}

TEST(RtpPayloadParamsTest, PictureIdWraps) {
  RtpPayloadState state;
  state.picture_id = kMaxTwoBytePictureId;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = kNoTemporalIdx;

  RtpPayloadParams params(kSsrc1, &state);
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(0,
            absl::get<RTPVideoHeaderVP8>(header.video_type_header).pictureId);

  // State should hold latest used picture id and tl0_pic_idx.
  EXPECT_EQ(0, params.state().picture_id);  // Wrapped.
  EXPECT_EQ(kInitialTl0PicIdx1, params.state().tl0_pic_idx);
}

TEST(RtpPayloadParamsTest, Tl0PicIdxUpdatedForVp8) {
  RtpPayloadState state;
  state.picture_id = kInitialPictureId1;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  // Modules are sending for this test.
  // OnEncodedImage, temporalIdx: 1.
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.temporalIdx = 1;

  RtpPayloadParams params(kSsrc1, &state);
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecVP8, header.codec);
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(header.video_type_header);
  EXPECT_EQ(kInitialPictureId1 + 1, vp8_header.pictureId);
  EXPECT_EQ(kInitialTl0PicIdx1, vp8_header.tl0PicIdx);

  // OnEncodedImage, temporalIdx: 0.
  codec_info.codecSpecific.VP8.temporalIdx = 0;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);
  EXPECT_EQ(kVideoCodecVP8, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, vp8_header.pictureId);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, vp8_header.tl0PicIdx);

  // State should hold latest used picture id and tl0_pic_idx.
  EXPECT_EQ(kInitialPictureId1 + 2, params.state().picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, params.state().tl0_pic_idx);
}

TEST(RtpPayloadParamsTest, Tl0PicIdxUpdatedForVp9) {
  RtpPayloadState state;
  state.picture_id = kInitialPictureId1;
  state.tl0_pic_idx = kInitialTl0PicIdx1;

  EncodedImage encoded_image;
  // Modules are sending for this test.
  // OnEncodedImage, temporalIdx: 1.
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.temporal_idx = 1;
  codec_info.codecSpecific.VP9.first_frame_in_picture = true;

  RtpPayloadParams params(kSsrc1, &state);
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  const auto& vp9_header =
      absl::get<RTPVideoHeaderVP9>(header.video_type_header);
  EXPECT_EQ(kInitialPictureId1 + 1, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1, vp9_header.tl0_pic_idx);

  // OnEncodedImage, temporalIdx: 0.
  codec_info.codecSpecific.VP9.temporal_idx = 0;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, vp9_header.tl0_pic_idx);

  // OnEncodedImage, first_frame_in_picture = false
  codec_info.codecSpecific.VP9.first_frame_in_picture = false;

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecVP9, header.codec);
  EXPECT_EQ(kInitialPictureId1 + 2, vp9_header.picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, vp9_header.tl0_pic_idx);

  // State should hold latest used picture id and tl0_pic_idx.
  EXPECT_EQ(kInitialPictureId1 + 2, params.state().picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1 + 1, params.state().tl0_pic_idx);
}

TEST(RtpPayloadParamsTest, PictureIdForOldGenericFormat) {
  test::ScopedFieldTrials generic_picture_id(
      "WebRTC-GenericPictureId/Enabled/");
  RtpPayloadState state{};

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info{};
  codec_info.codecType = kVideoCodecGeneric;

  RtpPayloadParams params(kSsrc1, &state);
  RTPVideoHeader header =
      params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);

  EXPECT_EQ(kVideoCodecGeneric, header.codec);
  ASSERT_TRUE(header.generic);
  EXPECT_EQ(0, header.generic->frame_id);

  header = params.GetRtpVideoHeader(encoded_image, &codec_info, kDontCare);
  ASSERT_TRUE(header.generic);
  EXPECT_EQ(1, header.generic->frame_id);
}

class RtpPayloadParamsVp8ToGenericTest : public ::testing::Test {
 public:
  enum LayerSync { kNoSync, kSync };

  enum TemporalLayerIndex { kTL0 = 0, kTL1 = 1, kTL2 = 2 };

  RtpPayloadParamsVp8ToGenericTest()
      : generic_descriptor_field_trial_("WebRTC-GenericDescriptor/Enabled/"),
        state_(),
        params_(123, &state_) {}

  static CodecSpecificInfo CreateCodecSpecificInfo(
      int temporal_index,
      LayerSync layer_sync,
      const std::vector<bool>& referenced_buffers,
      const std::vector<bool>& updated_buffers) {
    CodecSpecificInfo codec_info;
    codec_info.codecType = kVideoCodecVP8;

    auto& vp8_info = codec_info.codecSpecific.VP8;
    vp8_info.temporalIdx = temporal_index;
    vp8_info.layerSync = layer_sync == kSync;

    RTC_DCHECK_EQ(referenced_buffers.size(), kNumBuffers);
    RTC_DCHECK_EQ(updated_buffers.size(), kNumBuffers);
    for (size_t i = 0; i < kNumBuffers; ++i) {
      vp8_info.referencedBuffers[i] = referenced_buffers[i];
      vp8_info.updatedBuffers[i] = updated_buffers[i];
    }

    return codec_info;
  }

  void ConvertAndCheck(int64_t shared_frame_id,
                       TemporalLayerIndex temporal_index,
                       LayerSync layer_sync,
                       const std::vector<bool>& referenced_buffers,
                       const std::vector<bool>& updated_buffers,
                       const std::set<int64_t>& expected_deps,
                       FrameType frame_type,
                       uint16_t width,
                       uint16_t height) {
    RTC_DCHECK_EQ(referenced_buffers.size(), kNumBuffers);
    RTC_DCHECK_EQ(updated_buffers.size(), kNumBuffers);

    EncodedImage encoded_image;
    encoded_image._frameType = frame_type;
    encoded_image._encodedWidth = width;
    encoded_image._encodedHeight = height;

    CodecSpecificInfo codec_info = CreateCodecSpecificInfo(
        temporal_index, layer_sync, referenced_buffers, updated_buffers);

    RTPVideoHeader header =
        params_.GetRtpVideoHeader(encoded_image, &codec_info, shared_frame_id);

    ASSERT_TRUE(header.generic);
    EXPECT_TRUE(header.generic->higher_spatial_layers.empty());
    EXPECT_EQ(header.generic->spatial_index, 0);

    EXPECT_EQ(header.generic->frame_id, shared_frame_id);
    EXPECT_EQ(header.generic->temporal_index, temporal_index);
    std::set<int64_t> actual_deps(header.generic->dependencies.begin(),
                                  header.generic->dependencies.end());
    EXPECT_EQ(expected_deps, actual_deps);

    EXPECT_EQ(header.width, width);
    EXPECT_EQ(header.height, height);
  }

  void ConvertAndCheck(int64_t shared_frame_id,
                       TemporalLayerIndex temporal_index,
                       LayerSync layer_sync,
                       absl::optional<size_t> referenced_buffer,
                       const std::set<int64_t>& expected_deps,
                       FrameType frame_type = kVideoFrameDelta,
                       uint16_t width = 0,
                       uint16_t height = 0) {
    const bool is_key = !referenced_buffer;

    std::vector<bool> referenced_buffers(kNumBuffers);
    std::vector<bool> updated_buffers(kNumBuffers);

    for (size_t i = 0; i < kNumBuffers; ++i) {
      referenced_buffers[i] = referenced_buffer && i == *referenced_buffer;
      updated_buffers[i] = is_key || i == static_cast<size_t>(temporal_index);
    }

    return ConvertAndCheck(shared_frame_id, temporal_index, layer_sync,
                           referenced_buffers, updated_buffers, expected_deps,
                           frame_type, width, height);
  }

  void ConvertAndCheckKeyFrame(int64_t shared_frame_id,
                               uint16_t width = 480,
                               uint16_t height = 360) {
    return ConvertAndCheck(shared_frame_id, kTL0, kNoSync, absl::nullopt, {},
                           kVideoFrameKey, width, height);
  }

 protected:
  static constexpr size_t kNumBuffers = CodecSpecificInfoVP8::Buffer::kCount;

  test::ScopedFieldTrials generic_descriptor_field_trial_;
  RtpPayloadState state_;
  RtpPayloadParams params_;
};

TEST_F(RtpPayloadParamsVp8ToGenericTest, Keyframe) {
  ConvertAndCheckKeyFrame(0);
  ConvertAndCheck(1, kTL0, kNoSync, 1, {0});
  ConvertAndCheckKeyFrame(2);
}

TEST_F(RtpPayloadParamsVp8ToGenericTest, TooHighTemporalIndex) {
  ConvertAndCheckKeyFrame(0);

  EncodedImage encoded_image;
  encoded_image._frameType = kVideoFrameDelta;

  const int too_high_temporal_layer_index =
      RtpGenericFrameDescriptor::kMaxTemporalLayers;
  const std::vector<bool> referenced_buffers = {false, false, false};
  const std::vector<bool> updated_buffers = {true, true, true};
  CodecSpecificInfo codec_info =
      CreateCodecSpecificInfo(too_high_temporal_layer_index, kNoSync,
                              referenced_buffers, updated_buffers);

  RTPVideoHeader header =
      params_.GetRtpVideoHeader(encoded_image, &codec_info, 1);
  EXPECT_FALSE(header.generic);
}

TEST_F(RtpPayloadParamsVp8ToGenericTest, LayerSync) {
  // 02120212 pattern
  ConvertAndCheckKeyFrame(0);
  ConvertAndCheck(1, kTL2, kNoSync, 0, {0});
  ConvertAndCheck(2, kTL1, kNoSync, 0, {0});
  ConvertAndCheck(3, kTL2, kNoSync, 2, {1});

  ConvertAndCheck(4, kTL0, kNoSync, 0, {0});
  ConvertAndCheck(5, kTL2, kNoSync, 2, {3});
  ConvertAndCheck(6, kTL1, kSync, 0, {4});  // layer sync
  ConvertAndCheck(7, kTL2, kNoSync, 1, {6});
}

TEST_F(RtpPayloadParamsVp8ToGenericTest, FrameIdGaps) {
  // 0101 pattern
  ConvertAndCheckKeyFrame(0);
  ConvertAndCheck(1, kTL1, kNoSync, 0, {0});

  ConvertAndCheck(5, kTL0, kNoSync, 0, {0});
  ConvertAndCheck(10, kTL1, kNoSync, 1, {1});

  ConvertAndCheck(15, kTL0, kNoSync, 0, {5});
  ConvertAndCheck(20, kTL1, kNoSync, 1, {10});
}
}  // namespace webrtc
