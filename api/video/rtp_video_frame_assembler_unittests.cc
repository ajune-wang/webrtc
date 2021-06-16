/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "api/array_view.h"
#include "api/video/rtp_video_frame_assembler.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_packetizer_av1_unittest_helper.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Matches;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

class PacketBuilder {
 public:
  explicit PacketBuilder(VideoCodecType codec)
      : codec_(codec), packet_to_send_(&extension_manager_) {}

  PacketBuilder& AsRaw(bool raw = true) {
    raw_ = raw;
    return *this;
  }

  PacketBuilder& WithSeqNum(uint16_t seq_num) {
    seq_num_ = seq_num;
    return *this;
  }

  PacketBuilder& WithPayload(rtc::ArrayView<const uint8_t> payload) {
    payload_ = payload;
    return *this;
  }

  PacketBuilder& WithVideoHeader(const RTPVideoHeader& video_header) {
    video_header_ = video_header;
    return *this;
  }

  template <typename T, typename... Args>
  PacketBuilder& WithExtension(int id, const Args&... args) {
    extension_manager_.Register<T>(id);
    packet_to_send_.IdentifyExtensions(extension_manager_);
    packet_to_send_.SetExtension<T>(args...);
    return *this;
  }

  RtpPacketReceived Build() {
    auto packetizer = RtpPacketizer::Create(
        raw_ ? absl::nullopt : absl::optional<VideoCodecType>(codec_), payload_,
        {}, video_header_);
    packetizer->NextPacket(&packet_to_send_);
    packet_to_send_.SetSequenceNumber(seq_num_);

    RtpPacketReceived received(&extension_manager_);
    received.Parse(packet_to_send_.data(), packet_to_send_.size());
    return received;
  }

 private:
  VideoCodecType codec_;
  bool raw_ = false;
  uint16_t seq_num_ = 0;
  rtc::ArrayView<const uint8_t> payload_;
  RTPVideoHeader video_header_;
  RtpPacketReceived::ExtensionManager extension_manager_;
  RtpPacketToSend packet_to_send_;
};

void AppendFrames(RtpVideoFrameAssembler::ReturnVector from,
                  RtpVideoFrameAssembler::ReturnVector& to) {
  to.insert(to.end(), std::make_move_iterator(from.begin()),
            std::make_move_iterator(from.end()));
}

MATCHER_P(Payload, payload, "") {
  return Matches(ElementsAreArray(payload))(
      rtc::ArrayView<const uint8_t>(*arg->GetEncodedData()));
}

auto PayloadIs(rtc::ArrayView<uint8_t> payload) {
  return Payload(payload);
}

MATCHER_P2(IdAndRefs, id, refs, "") {
  return Matches(Eq(id))(arg->Id()) &&
         Matches(UnorderedElementsAreArray(refs))(
             rtc::ArrayView<int64_t>(arg->references, arg->num_references));
}

auto IdAndRefsAre(int64_t frame_id, const std::vector<int64_t>& refs) {
  return IdAndRefs(frame_id, refs);
}

TEST(RtpVideoFrameAssembler, Vp8Packetization) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kVp8);

  // IMPORTANT! When sending VP8 over RTP parts of the payload is actually
  // inspected at the RTP level. It just so happen that the initial 'V' set
  // keyframe bit (0x01) to the correct value.
  uint8_t kKeyframePayload[] = "Vp8Keyframe";
  ASSERT_EQ(kKeyframePayload[0] & 0x01, 0);

  uint8_t kDeltaframePayload[] = "SomeFrame";
  ASSERT_EQ(kDeltaframePayload[0] & 0x01, 1);

  RtpVideoFrameAssembler::ReturnVector frames;

  RTPVideoHeader video_header;
  auto& generic = video_header.video_type_header.emplace<RTPVideoHeaderVP8>();

  generic.pictureId = 10;
  generic.tl0PicIdx = 0;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecVP8)
                                          .WithPayload(kKeyframePayload)
                                          .WithVideoHeader(video_header)
                                          .Build()),
               frames);

  generic.pictureId = 11;
  generic.tl0PicIdx = 1;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecVP8)
                                          .WithPayload(kDeltaframePayload)
                                          .WithVideoHeader(video_header)
                                          .Build()),
               frames);

  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[0], PayloadIs(kKeyframePayload));
  EXPECT_THAT(frames[0], IdAndRefsAre(10, {}));
  EXPECT_THAT(frames[1], PayloadIs(kDeltaframePayload));
  EXPECT_THAT(frames[1], IdAndRefsAre(11, {10}));
}

TEST(RtpVideoFrameAssembler, Vp9Packetization) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kVp9);
  RtpVideoFrameAssembler::ReturnVector frames;

  uint8_t kPayload[] = "SomePayload";

  RTPVideoHeader video_header;
  auto& generic = video_header.video_type_header.emplace<RTPVideoHeaderVP9>();
  generic.InitRTPVideoHeaderVP9();

  generic.picture_id = 10;
  generic.tl0_pic_idx = 0;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecVP9)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .Build()),
               frames);

  generic.picture_id = 11;
  generic.tl0_pic_idx = 1;
  generic.inter_pic_predicted = true;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecVP9)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .Build()),
               frames);

  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[0], PayloadIs(kPayload));
  EXPECT_THAT(frames[0], IdAndRefsAre(10, {}));
  EXPECT_THAT(frames[1], PayloadIs(kPayload));
  EXPECT_THAT(frames[1], IdAndRefsAre(11, {10}));
}

TEST(RtpVideoFrameAssembler, Av1Packetization) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kAv1);
  RtpVideoFrameAssembler::ReturnVector frames;

  auto kKeyframePayload = test::BuildAv1Frame(
      {test::Obu(test::kObuTypeSequenceHeader).WithPayload({1, 2, 3}),
       test::Obu(test::kObuTypeFrame).WithPayload({4, 5, 6})});

  auto kDeltaframePayload = test::BuildAv1Frame(
      {test::Obu(test::kObuTypeFrame).WithPayload({7, 8, 9})});

  RTPVideoHeader video_header;

  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecAV1)
                                          .WithPayload(kKeyframePayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(20)
                                          .Build()),
               frames);

  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecAV1)
                                          .WithPayload(kDeltaframePayload)
                                          .WithSeqNum(21)
                                          .Build()),
               frames);

  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[0], PayloadIs(kKeyframePayload));
  EXPECT_THAT(frames[0], IdAndRefsAre(20, {}));
  EXPECT_THAT(frames[1], PayloadIs(kDeltaframePayload));
  EXPECT_THAT(frames[1], IdAndRefsAre(21, {20}));
}

TEST(RtpVideoFrameAssembler, RawPacketizationDependencyDescriptorExtension) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kRaw);
  RtpVideoFrameAssembler::ReturnVector frames;
  uint8_t kPayload[] = "SomePayload";

  FrameDependencyStructure dependency_structure;
  dependency_structure.num_decode_targets = 1;
  dependency_structure.num_chains = 1;
  dependency_structure.decode_target_protected_by_chain.push_back(0);
  dependency_structure.templates.push_back(
      FrameDependencyTemplate().S(0).T(0).Dtis("S").ChainDiffs({0}));
  dependency_structure.templates.push_back(
      FrameDependencyTemplate().S(0).T(0).Dtis("S").ChainDiffs({10}).FrameDiffs(
          {10}));

  DependencyDescriptor dependency_descriptor;

  dependency_descriptor.frame_number = 10;
  dependency_descriptor.frame_dependencies = dependency_structure.templates[0];
  dependency_descriptor.attached_structure =
      std::make_unique<FrameDependencyStructure>(dependency_structure);
  AppendFrames(assembler.InsertPacket(
                   PacketBuilder(kVideoCodecAV1)
                       .WithPayload(kPayload)
                       .WithExtension<RtpDependencyDescriptorExtension>(
                           1, dependency_structure, dependency_descriptor)
                       .AsRaw()
                       .Build()),
               frames);

  dependency_descriptor.frame_number = 20;
  dependency_descriptor.frame_dependencies = dependency_structure.templates[1];
  dependency_descriptor.attached_structure.reset();
  AppendFrames(assembler.InsertPacket(
                   PacketBuilder(kVideoCodecAV1)
                       .WithPayload(kPayload)
                       .WithExtension<RtpDependencyDescriptorExtension>(
                           1, dependency_structure, dependency_descriptor)
                       .AsRaw()
                       .Build()),
               frames);

  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[0], PayloadIs(kPayload));
  EXPECT_THAT(frames[0], IdAndRefsAre(10, {}));
  EXPECT_THAT(frames[1], PayloadIs(kPayload));
  EXPECT_THAT(frames[1], IdAndRefsAre(20, {10}));
}

TEST(RtpVideoFrameAssembler, RawPacketizationGenericDescriptor00Extension) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kRaw);
  RtpVideoFrameAssembler::ReturnVector frames;
  uint8_t kPayload[] = "SomePayload";

  RtpGenericFrameDescriptor generic;

  generic.SetFirstPacketInSubFrame(true);
  generic.SetLastPacketInSubFrame(true);
  generic.SetFrameId(100);
  AppendFrames(
      assembler.InsertPacket(
          PacketBuilder(kVideoCodecAV1)
              .WithPayload(kPayload)
              .WithExtension<RtpGenericFrameDescriptorExtension00>(1, generic)
              .AsRaw()
              .Build()),
      frames);

  generic.SetFrameId(102);
  generic.AddFrameDependencyDiff(2);
  AppendFrames(
      assembler.InsertPacket(
          PacketBuilder(kVideoCodecAV1)
              .WithPayload(kPayload)
              .WithExtension<RtpGenericFrameDescriptorExtension00>(1, generic)
              .AsRaw()
              .Build()),
      frames);

  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[0], PayloadIs(kPayload));
  EXPECT_THAT(frames[0], IdAndRefsAre(100, {}));
  EXPECT_THAT(frames[1], PayloadIs(kPayload));
  EXPECT_THAT(frames[1], IdAndRefsAre(102, {100}));
}

TEST(RtpVideoFrameAssembler, RawPacketizationGenericPayloadDescriptor) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kGeneric);
  RtpVideoFrameAssembler::ReturnVector frames;
  uint8_t kPayload[] = "SomePayload";

  RTPVideoHeader video_header;

  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecGeneric)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(123)
                                          .Build()),
               frames);

  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecGeneric)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(124)
                                          .Build()),
               frames);

  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[0], PayloadIs(kPayload));
  EXPECT_THAT(frames[0], IdAndRefsAre(123, {}));
  EXPECT_THAT(frames[1], PayloadIs(kPayload));
  EXPECT_THAT(frames[1], IdAndRefsAre(124, {123}));
}

TEST(RtpVideoFrameAssembler, Padding) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kGeneric);
  RtpVideoFrameAssembler::ReturnVector frames;
  uint8_t kPayload[] = "SomePayload";

  RTPVideoHeader video_header;

  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecGeneric)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(123)
                                          .Build()),
               frames);

  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecGeneric)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(125)
                                          .Build()),
               frames);

  ASSERT_THAT(frames, SizeIs(1));
  EXPECT_THAT(frames[0], PayloadIs(kPayload));
  EXPECT_THAT(frames[0], IdAndRefsAre(123, {}));

  // Padding packets have no bitstream data. An easy way to generate one is to
  // build a normal packet and then simply remove the bitstream portion of the
  // payload.
  RtpPacketReceived padding_packet = PacketBuilder(kVideoCodecGeneric)
                                         .WithPayload(kPayload)
                                         .WithVideoHeader(video_header)
                                         .WithSeqNum(124)
                                         .Build();
  // The payload descriptor is one byte, keep it.
  padding_packet.SetPayloadSize(1);

  AppendFrames(assembler.InsertPacket(padding_packet), frames);

  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[1], PayloadIs(kPayload));
  EXPECT_THAT(frames[1], IdAndRefsAre(125, {123}));
}

TEST(RtpVideoFrameAssembler, ClearTo) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kGeneric);
  RtpVideoFrameAssembler::ReturnVector frames;
  uint8_t kPayload[] = "SomePayload";

  RTPVideoHeader video_header;

  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecGeneric)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(123)
                                          .Build()),
               frames);

  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecGeneric)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(125)
                                          .Build()),
               frames);

  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecGeneric)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(126)
                                          .Build()),
               frames);

  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[0], IdAndRefsAre(123, {}));
  EXPECT_THAT(frames[1], IdAndRefsAre(126, {}));

  assembler.ClearTo(126);

  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  AppendFrames(assembler.InsertPacket(PacketBuilder(kVideoCodecGeneric)
                                          .WithPayload(kPayload)
                                          .WithVideoHeader(video_header)
                                          .WithSeqNum(124)
                                          .Build()),
               frames);

  ASSERT_THAT(frames, SizeIs(2));
}

}  // namespace
}  // namespace webrtc
