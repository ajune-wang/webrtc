/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/h264_packet_buffer.h"

#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <utility>

#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;

using H264::NaluType::kAud;
using H264::NaluType::kFuA;
using H264::NaluType::kIdr;
using H264::NaluType::kPps;
using H264::NaluType::kSlice;
using H264::NaluType::kSps;
using H264::NaluType::kStapA;

constexpr uint8_t kStartCode[] = {0, 0, 0, 1};
constexpr int kBufferSize = 2048;

void IgnoreResult(H264PacketBuffer::InsertResult /*result*/) {}

NaluInfo MakeNaluInfo(uint8_t type) {
  NaluInfo res;
  res.type = type;
  res.sps_id = -1;
  res.pps_id = -1;
  return res;
}

class Packet {
 public:
  explicit Packet(H264PacketizationTypes type);

  Packet& Idr(std::vector<uint8_t> payload = {9, 9, 9});
  Packet& Slice(std::vector<uint8_t> payload = {9, 9, 9});
  Packet& Sps(std::vector<uint8_t> payload = {9, 9, 9});
  Packet& SpsWithResolution(int width,
                            int height,
                            std::vector<uint8_t> payload = {9, 9, 9});
  Packet& Pps(std::vector<uint8_t> payload = {9, 9, 9});
  Packet& Aud();
  Packet& Marker();
  Packet& AsFirstFragment();
  Packet& Time(uint32_t rtp_timestamp);
  Packet& SeqNum(uint16_t rtp_seq_num);

  operator std::unique_ptr<H264PacketBuffer::Packet>();

 private:
  rtc::CopyOnWriteBuffer BuildFuaPayload() const;
  rtc::CopyOnWriteBuffer BuildSingleNaluPayload() const;
  rtc::CopyOnWriteBuffer BuildStapAPayload() const;

  H264PacketizationTypes type_;
  RTPVideoHeader video_header_;
  RTPVideoHeaderH264& h264_header_;
  bool first_fragment_ = false;
  bool marker_bit_ = false;
  uint32_t rtp_timestamp_ = 0;
  uint16_t rtp_seq_num_ = 0;
  std::vector<std::vector<uint8_t>> nalu_payloads_;
};

Packet::Packet(H264PacketizationTypes type)
    : type_(type),
      h264_header_(
          video_header_.video_type_header.emplace<RTPVideoHeaderH264>()) {}

Packet& Packet::Idr(std::vector<uint8_t> payload) {
  h264_header_.nalus[h264_header_.nalus_length++] = MakeNaluInfo(kIdr);
  nalu_payloads_.emplace_back(payload.begin(), payload.end());
  return *this;
}

Packet& Packet::Slice(std::vector<uint8_t> payload) {
  h264_header_.nalus[h264_header_.nalus_length++] = MakeNaluInfo(kSlice);
  nalu_payloads_.emplace_back(payload.begin(), payload.end());
  return *this;
}

Packet& Packet::Sps(std::vector<uint8_t> payload) {
  h264_header_.nalus[h264_header_.nalus_length++] = MakeNaluInfo(kSps);
  nalu_payloads_.emplace_back(payload.begin(), payload.end());
  return *this;
}

Packet& Packet::SpsWithResolution(int width,
                                  int height,
                                  std::vector<uint8_t> payload) {
  h264_header_.nalus[h264_header_.nalus_length++] = MakeNaluInfo(kSps);
  video_header_.width = width;
  video_header_.height = height;
  nalu_payloads_.emplace_back(payload.begin(), payload.end());
  return *this;
}

Packet& Packet::Pps(std::vector<uint8_t> payload) {
  h264_header_.nalus[h264_header_.nalus_length++] = MakeNaluInfo(kPps);
  nalu_payloads_.emplace_back(payload.begin(), payload.end());
  return *this;
}

Packet& Packet::Aud() {
  h264_header_.nalus[h264_header_.nalus_length++] = MakeNaluInfo(kAud);
  nalu_payloads_.push_back({});
  return *this;
}

Packet& Packet::Marker() {
  marker_bit_ = true;
  return *this;
}

Packet& Packet::AsFirstFragment() {
  first_fragment_ = true;
  return *this;
}

Packet& Packet::Time(uint32_t rtp_timestamp) {
  rtp_timestamp_ = rtp_timestamp;
  return *this;
}

Packet& Packet::SeqNum(uint16_t rtp_seq_num) {
  rtp_seq_num_ = rtp_seq_num;
  return *this;
}

Packet::operator std::unique_ptr<H264PacketBuffer::Packet>() {
  auto res = std::make_unique<H264PacketBuffer::Packet>();

  switch (type_) {
    case kH264FuA: {
      RTC_CHECK_EQ(h264_header_.nalus_length, 1);
      res->video_payload = BuildFuaPayload();
      break;
    }
    case kH264SingleNalu: {
      RTC_CHECK_EQ(h264_header_.nalus_length, 1);
      res->video_payload = BuildSingleNaluPayload();
      break;
    }
    case kH264StapA: {
      RTC_CHECK_GT(h264_header_.nalus_length, 1);
      RTC_CHECK_LE(h264_header_.nalus_length, kMaxNalusPerPacket);
      res->video_payload = BuildStapAPayload();
      break;
    }
  }

  if (type_ == kH264FuA && !first_fragment_) {
    h264_header_.nalus_length = 0;
  }

  h264_header_.packetization_type = type_;
  res->marker_bit = marker_bit_;
  res->video_header = video_header_;
  res->timestamp = rtp_timestamp_;
  res->seq_num = rtp_seq_num_;
  res->video_header.codec = kVideoCodecH264;

  return res;
}

rtc::CopyOnWriteBuffer Packet::BuildFuaPayload() const {
  constexpr int kStartBit = 0x80;

  rtc::CopyOnWriteBuffer res;
  const uint8_t indicator = H264::NaluType::kFuA;
  res.AppendData(&indicator, 1);
  const uint8_t header =
      h264_header_.nalus[0].type | (first_fragment_ ? kStartBit : 0);
  res.AppendData(&header, 1);
  res.AppendData(nalu_payloads_[0].data(), nalu_payloads_[0].size());
  return res;
}

rtc::CopyOnWriteBuffer Packet::BuildSingleNaluPayload() const {
  rtc::CopyOnWriteBuffer res;
  res.AppendData(&h264_header_.nalus[0].type, 1);
  res.AppendData(nalu_payloads_[0].data(), nalu_payloads_[0].size());
  return res;
}

rtc::CopyOnWriteBuffer Packet::BuildStapAPayload() const {
  rtc::CopyOnWriteBuffer res;

  const uint8_t indicator = H264::NaluType::kStapA;
  res.AppendData(&indicator, 1);

  for (size_t i = 0; i < h264_header_.nalus_length; ++i) {
    // The two first bytes indicates the nalu segment size.
    uint8_t length_as_array[2] = {
        0, static_cast<uint8_t>(nalu_payloads_[i].size() + 1)};
    res.AppendData(length_as_array);

    res.AppendData(&h264_header_.nalus[i].type, 1);
    res.AppendData(nalu_payloads_[i].data(), nalu_payloads_[i].size());
  }
  return res;
}

rtc::ArrayView<const uint8_t> PacketPayload(
    const H264PacketBuffer::InsertResult& res,
    int index) {
  return rtc::ArrayView<const uint8_t>(res.packets[index]->video_payload);
}

struct ArrayElem {
  template <size_t N>
  ArrayElem(const uint8_t (&d)[N])  // NOLINT(runtime/explicit)
      : data(std::begin(d), std::end(d)) {}
  ArrayElem(std::initializer_list<uint8_t> d)  // NOLINT(runtime/explicit)
      : data(d.begin(), d.end()) {}
  ArrayElem(uint8_t d) : data(1, d) {}  // NOLINT(runtime/explicit)
  std::vector<uint8_t> data;
};

std::vector<uint8_t> BuildVector(const std::vector<ArrayElem>& elems) {
  std::vector<uint8_t> res;
  for (auto& elem : elems) {
    res.insert(res.end(), elem.data.begin(), elem.data.end());
  }
  return res;
}

TEST(H264PacketBufferTest, IdrIsKeyframe) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/true);

  EXPECT_THAT(packet_buffer.InsertPacket(Packet(kH264SingleNalu).Idr().Marker())
                  .packets,
              SizeIs(1));
}

TEST(H264PacketBufferTest, IdrIsNotKeyframe) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(packet_buffer.InsertPacket(Packet(kH264SingleNalu).Idr().Marker())
                  .packets,
              IsEmpty());
}

TEST(H264PacketBufferTest, IdrIsKeyframeFuaRequiresFirstFragmet) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/true);

  // Not marked as the first fragment
  EXPECT_THAT(
      packet_buffer.InsertPacket(Packet(kH264FuA).Idr().SeqNum(0).Time(0))
          .packets,
      IsEmpty());

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264FuA).Idr().SeqNum(1).Time(0).Marker())
          .packets,
      IsEmpty());

  // Marked as first fragment
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264FuA).Idr().SeqNum(2).Time(1).AsFirstFragment())
          .packets,
      IsEmpty());

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264FuA).Idr().SeqNum(3).Time(1).Marker())
          .packets,
      SizeIs(2));
}

TEST(H264PacketBufferTest, IdrSpsPpsIsKeyframeSingleNalus) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  // No SPS
  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Pps().SeqNum(1).Time(1)));
  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264SingleNalu).Idr().SeqNum(2).Time(1).Marker())
                  .packets,
              IsEmpty());

  // No PPS
  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Sps().SeqNum(3).Time(2)));
  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264SingleNalu).Idr().SeqNum(4).Time(2).Marker())
                  .packets,
              IsEmpty());

  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Sps().SeqNum(5).Time(3)));
  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Pps().SeqNum(6).Time(3)));
  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264SingleNalu).Idr().SeqNum(7).Time(3).Marker())
                  .packets,
              SizeIs(3));
}

TEST(H264PacketBufferTest, IdrSpsPpsIsKeyframeStapA) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  // No SPS
  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264StapA).Pps().Idr().SeqNum(1).Time(1).Marker())
                  .packets,
              IsEmpty());

  // No PPS
  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264StapA).Sps().Idr().SeqNum(2).Time(2).Marker())
                  .packets,
              IsEmpty());

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264StapA).Sps().Pps().Idr().SeqNum(3).Time(3).Marker())
          .packets,
      SizeIs(1));
}

TEST(H264PacketBufferTest, InsertingSpsPpsLastGeneratesKeyframe) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Idr().SeqNum(2).Time(1).Marker()));

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264StapA).Sps().Pps().SeqNum(1).Time(1))
          .packets,
      SizeIs(2));
}

TEST(H264PacketBufferTest, InsertingMidFuaCompletesFrame) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264StapA).Sps().Pps().Idr().SeqNum(0).Time(0).Marker())
          .packets,
      SizeIs(1));

  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264FuA).Slice().SeqNum(1).Time(1).AsFirstFragment()));
  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264FuA).Slice().SeqNum(3).Time(1).Marker()));
  EXPECT_THAT(
      packet_buffer.InsertPacket(Packet(kH264FuA).Slice().SeqNum(2).Time(1))
          .packets,
      SizeIs(3));
}

TEST(H264PacketBufferTest, SeqNumJumpDoesNotCompleteFrame) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264StapA).Sps().Pps().Idr().SeqNum(0).Time(0).Marker())
          .packets,
      SizeIs(1));

  EXPECT_THAT(
      packet_buffer.InsertPacket(Packet(kH264FuA).Slice().SeqNum(1).Time(1))
          .packets,
      IsEmpty());

  // Add `kBufferSize` to make the index of the sequence number wrap and end up
  // where the packet with sequence number 2 would have ended up.
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264FuA).Slice().SeqNum(2 + kBufferSize).Time(3).Marker())
          .packets,
      IsEmpty());
}

TEST(H264PacketBufferTest, DifferentTimestampsDoesNotCompleteFrame) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264StapA).Sps().Pps().SeqNum(0).Time(0))
          .packets,
      IsEmpty());

  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264SingleNalu).Idr().SeqNum(1).Time(1).Marker())
                  .packets,
              IsEmpty());
}

TEST(H264PacketBufferTest, FrameBoundariesAreSet) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  auto key = packet_buffer.InsertPacket(
      Packet(kH264StapA).Sps().Pps().Idr().SeqNum(1).Time(1).Marker());

  ASSERT_THAT(key.packets, SizeIs(1));
  EXPECT_TRUE(key.packets[0]->video_header.is_first_packet_in_frame);
  EXPECT_TRUE(key.packets[0]->video_header.is_last_packet_in_frame);

  IgnoreResult(
      packet_buffer.InsertPacket(Packet(kH264FuA).Slice().SeqNum(2).Time(2)));
  IgnoreResult(
      packet_buffer.InsertPacket(Packet(kH264FuA).Slice().SeqNum(3).Time(2)));
  auto delta = packet_buffer.InsertPacket(
      Packet(kH264FuA).Slice().SeqNum(4).Time(2).Marker());

  ASSERT_THAT(delta.packets, SizeIs(3));
  EXPECT_TRUE(delta.packets[0]->video_header.is_first_packet_in_frame);
  EXPECT_FALSE(delta.packets[0]->video_header.is_last_packet_in_frame);

  EXPECT_FALSE(delta.packets[1]->video_header.is_first_packet_in_frame);
  EXPECT_FALSE(delta.packets[1]->video_header.is_last_packet_in_frame);

  EXPECT_FALSE(delta.packets[2]->video_header.is_first_packet_in_frame);
  EXPECT_TRUE(delta.packets[2]->video_header.is_last_packet_in_frame);
}

TEST(H264PacketBufferTest, ResolutionSetOnFirstPacket) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Aud().SeqNum(1).Time(1)));
  auto res = packet_buffer.InsertPacket(Packet(kH264StapA)
                                            .SpsWithResolution(320, 240)
                                            .Pps()
                                            .Idr()
                                            .SeqNum(2)
                                            .Time(1)
                                            .Marker());

  ASSERT_THAT(res.packets, SizeIs(2));
  EXPECT_THAT(res.packets[0]->video_header.width, Eq(320));
  EXPECT_THAT(res.packets[0]->video_header.height, Eq(240));
}

TEST(H264PacketBufferTest, KeyframeAndDeltaFrameSetOnFirstPacket) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Aud().SeqNum(1).Time(1)));
  auto key = packet_buffer.InsertPacket(
      Packet(kH264StapA).Sps().Pps().Idr().SeqNum(2).Time(1).Marker());

  auto delta = packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Slice().SeqNum(3).Time(2).Marker());

  ASSERT_THAT(key.packets, SizeIs(2));
  EXPECT_THAT(key.packets[0]->video_header.frame_type,
              Eq(VideoFrameType::kVideoFrameKey));
  ASSERT_THAT(delta.packets, SizeIs(1));
  EXPECT_THAT(delta.packets[0]->video_header.frame_type,
              Eq(VideoFrameType::kVideoFrameDelta));
}

TEST(H264PacketBufferTest, RtpSeqNumWrap) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264StapA).Sps().Pps().SeqNum((1 << 16) - 1).Time(0)));

  IgnoreResult(
      packet_buffer.InsertPacket(Packet(kH264FuA).Idr().SeqNum(0).Time(0)));
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264FuA).Idr().SeqNum(1).Time(0).Marker())
          .packets,
      SizeIs(3));
}

TEST(H264PacketBufferTest, StapaFixedBitstream) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  auto res = packet_buffer.InsertPacket(Packet(kH264StapA)
                                            .Sps({1, 2, 3})
                                            .Pps({4, 5, 6})
                                            .Idr({7, 8, 9})
                                            .SeqNum(0)
                                            .Time(0)
                                            .Marker());
  auto expected = BuildVector({kStartCode,
                               kSps,
                               {1, 2, 3},
                               kStartCode,
                               kPps,
                               {4, 5, 6},
                               kStartCode,
                               kIdr,
                               {7, 8, 9}});

  ASSERT_THAT(res.packets, SizeIs(1));
  EXPECT_THAT(PacketPayload(res, 0), ElementsAreArray(expected));
}

TEST(H264PacketBufferTest, SingleNaluFixedBitstream) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Sps({1, 2, 3}).SeqNum(0).Time(0)));
  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Pps({4, 5, 6}).SeqNum(1).Time(0)));
  auto res = packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Idr({7, 8, 9}).SeqNum(2).Time(0).Marker());

  auto expected_first = BuildVector({kStartCode, kSps, {1, 2, 3}});
  auto expected_second = BuildVector({kStartCode, kPps, {4, 5, 6}});
  auto expected_third = BuildVector({kStartCode, kIdr, {7, 8, 9}});

  ASSERT_THAT(res.packets, SizeIs(3));
  EXPECT_THAT(PacketPayload(res, 0), ElementsAreArray(expected_first));
  EXPECT_THAT(PacketPayload(res, 1), ElementsAreArray(expected_second));
  EXPECT_THAT(PacketPayload(res, 2), ElementsAreArray(expected_third));
}

TEST(H264PacketBufferTest, StapaAndFuaFixedBitstream) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264StapA).Sps({1, 2, 3}).Pps({4, 5, 6}).SeqNum(0).Time(0)));
  IgnoreResult(packet_buffer.InsertPacket(
      Packet(kH264FuA).Idr({8, 8, 8}).SeqNum(1).Time(0).AsFirstFragment()));
  auto res = packet_buffer.InsertPacket(
      Packet(kH264FuA).Idr({9, 9, 9}).SeqNum(2).Time(0).Marker());

  auto expected_first =
      BuildVector({kStartCode, kSps, {1, 2, 3}, kStartCode, kPps, {4, 5, 6}});
  auto expected_second = BuildVector({kStartCode, kIdr, {8, 8, 8}});
  // Third is a continuation of second, so only the payload is expected.
  auto expected_third = BuildVector({{9, 9, 9}});

  ASSERT_THAT(res.packets, SizeIs(3));
  EXPECT_THAT(PacketPayload(res, 0), ElementsAreArray(expected_first));
  EXPECT_THAT(PacketPayload(res, 1), ElementsAreArray(expected_second));
  EXPECT_THAT(PacketPayload(res, 2), ElementsAreArray(expected_third));
}

TEST(H264PacketBufferTest, FullPacketBufferDoesNotBlockKeyframe) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  for (int i = 0; i < kBufferSize; ++i) {
    EXPECT_THAT(
        packet_buffer
            .InsertPacket(Packet(kH264SingleNalu).Slice().SeqNum(i).Time(0))
            .packets,
        IsEmpty());
  }

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(kBufferSize)
                                    .Time(1)
                                    .Marker())
                  .packets,
              SizeIs(1));
}

TEST(H264PacketBufferTest, TooManyNalusInPacket) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  std::unique_ptr<H264PacketBuffer::Packet> packet(
      Packet(kH264StapA).Sps().Pps().Idr().SeqNum(1).Time(1).Marker());
  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(packet->video_header.video_type_header);
  h264_header.nalus_length = kMaxNalusPerPacket + 1;

  EXPECT_THAT(packet_buffer.InsertPacket(std::move(packet)).packets, IsEmpty());
}

}  // namespace
}  // namespace webrtc
