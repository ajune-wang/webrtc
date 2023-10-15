/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_stats_impl.h"

#include <tuple>

#include "absl/types/optional.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::Values;
using Filter = VideoCodecStats::Filter;
using Frame = VideoCodecStatsImpl::Frame;
using Stream = VideoCodecStats::Stream;
using EncodingSettings = VideoCodecTester::EncodingSettings;

class TestFrame {
 public:
  TestFrame& Fn(int frame_num) {
    frame_.frame_num = frame_num;
    return *this;
  }
  TestFrame& Ts(int timestamp_rtp) {
    frame_.timestamp_rtp = timestamp_rtp;
    return *this;
  }
  TestFrame& S(int spatial_idx) {
    frame_.spatial_idx = spatial_idx;
    return *this;
  }
  TestFrame& T(int temporal_idx) {
    frame_.temporal_idx = temporal_idx;
    return *this;
  }
  TestFrame& Tsi(std::set<int> target_spatial_idxs) {
    frame_.target_spatial_idxs = target_spatial_idxs;
    return *this;
  }
  TestFrame& Tti(std::set<int> target_temporal_idxs) {
    frame_.target_temporal_idxs = target_temporal_idxs;
    return *this;
  }
  TestFrame& Sz(int num_bytes) {
    frame_.frame_size = DataSize::Bytes(num_bytes);
    return *this;
  }
  TestFrame& Kf(bool keyframe) {
    frame_.keyframe = keyframe;
    return *this;
  }
  TestFrame& Es(EncodingSettings encoding_settings) {
    frame_.encoding_settings = encoding_settings;
    return *this;
  }
  operator Frame() { return frame_; }
  Frame frame_;
};

}  // namespace

TEST(VideoCodecStatsImpl, AddAndGetFrame) {
  VideoCodecStatsImpl stats;
  stats.AddFrame({.timestamp_rtp = 0, .spatial_idx = 0});
  stats.AddFrame({.timestamp_rtp = 0, .spatial_idx = 1});
  stats.AddFrame({.timestamp_rtp = 1, .spatial_idx = 0});

  Frame* fs = stats.GetFrame(/*timestamp_rtp=*/0, /*spatial_idx=*/0);
  ASSERT_NE(fs, nullptr);
  EXPECT_EQ(fs->timestamp_rtp, 0u);
  EXPECT_EQ(fs->spatial_idx, 0);

  fs = stats.GetFrame(/*timestamp_rtp=*/0, /*spatial_idx=*/1);
  ASSERT_NE(fs, nullptr);
  EXPECT_EQ(fs->timestamp_rtp, 0u);
  EXPECT_EQ(fs->spatial_idx, 1);

  fs = stats.GetFrame(/*timestamp_rtp=*/1, /*spatial_idx=*/0);
  ASSERT_NE(fs, nullptr);
  EXPECT_EQ(fs->timestamp_rtp, 1u);
  EXPECT_EQ(fs->spatial_idx, 0);

  fs = stats.GetFrame(/*timestamp_rtp=*/1, /*spatial_idx=*/1);
  EXPECT_EQ(fs, nullptr);
}

class VideoCodecStatsImplSlicingTest
    : public ::testing::TestWithParam<std::tuple<Filter, std::vector<int>>> {};

TEST_P(VideoCodecStatsImplSlicingTest, Slice) {
  Filter filter = std::get<0>(GetParam());
  std::vector<int> expected_frames = std::get<1>(GetParam());
  std::vector<VideoCodecStats::Frame> frames = {
      TestFrame().Fn(0).Ts(0).S(0).T(0).Tsi({0, 1}).Tti({0, 1}),
      TestFrame().Fn(0).Ts(0).S(1).T(0).Tsi({1}).Tti({0, 1}),
      TestFrame().Fn(1).Ts(1).S(0).T(1).Tsi({0}).Tti({1}),
      TestFrame().Fn(1).Ts(1).S(1).T(1).Tsi({1}).Tti({1})};

  VideoCodecStatsImpl stats;
  stats.AddFrame(frames[0]);
  stats.AddFrame(frames[1]);
  stats.AddFrame(frames[2]);
  stats.AddFrame(frames[3]);

  std::vector<VideoCodecStats::Frame> slice = stats.Slice(filter);
  ASSERT_EQ(slice.size(), expected_frames.size());
  for (size_t i = 0; i < expected_frames.size(); ++i) {
    Frame& expected = frames[expected_frames[i]];
    EXPECT_EQ(slice[i].frame_num, expected.frame_num);
    EXPECT_EQ(slice[i].timestamp_rtp, expected.timestamp_rtp);
    EXPECT_EQ(slice[i].spatial_idx, expected.spatial_idx);
    EXPECT_EQ(slice[i].temporal_idx, expected.temporal_idx);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCodecStatsImplSlicingTest,
    ::testing::Values(
        std::make_tuple(Filter{}, std::vector<int>{0, 1, 2, 3}),
        std::make_tuple(Filter{.first_frame = 1}, std::vector<int>{2, 3}),
        std::make_tuple(Filter{.last_frame = 0}, std::vector<int>{0, 1}),
        std::make_tuple(Filter{.spatial_idx = 0}, std::vector<int>{0, 2}),
        std::make_tuple(Filter{.spatial_idx = 1}, std::vector<int>{0, 1, 3}),
        std::make_tuple(Filter{.temporal_idx = 0}, std::vector<int>{0, 1}),
        std::make_tuple(Filter{.temporal_idx = 1},
                        std::vector<int>{0, 1, 2, 3})));

TEST(VideoCodecStatsImpl, Merge) {
  std::vector<VideoCodecStats::Frame> frames = {
      TestFrame().Fn(0).Ts(0).S(0).T(0).Sz(1).Kf(true),
      TestFrame().Fn(0).Ts(0).S(1).T(0).Sz(2),
      TestFrame().Fn(1).Ts(1).S(0).T(1).Sz(4),
      TestFrame().Fn(1).Ts(1).S(1).T(1).Sz(8)};
  std::vector<VideoCodecStats::Frame> sframes =
      VideoCodecStatsImpl::Merge(frames);
  ASSERT_THAT(sframes, SizeIs(2));
  EXPECT_EQ(sframes[0].spatial_idx, 1);
  EXPECT_EQ(sframes[0].keyframe, true);
  EXPECT_EQ(sframes[0].frame_size, DataSize::Bytes(3));
  EXPECT_EQ(sframes[1].spatial_idx, 1);
  EXPECT_EQ(sframes[1].keyframe, false);
  EXPECT_EQ(sframes[0].frame_size, DataSize::Bytes(3));
}

TEST(VideoCodecStatsImpl, MergeTargetRates) {
  EncodingSettings es{
      .sdp_video_format = SdpVideoFormat("Foo"),
      .scalability_mode = ScalabilityMode::kL2T2_KEY,
      .layer_settings = {{{.spatial_idx = 0, .temporal_idx = 0},
                          {.framerate = Frequency::Hertz(1),
                           .bitrate = DataRate::KilobitsPerSec(1)}},
                         {{.spatial_idx = 0, .temporal_idx = 1},
                          {.framerate = Frequency::Hertz(2),
                           .bitrate = DataRate::KilobitsPerSec(2)}},
                         {{.spatial_idx = 1, .temporal_idx = 0},
                          {.framerate = Frequency::Hertz(1),
                           .bitrate = DataRate::KilobitsPerSec(4)}},
                         {{.spatial_idx = 1, .temporal_idx = 1},
                          {.framerate = Frequency::Hertz(2),
                           .bitrate = DataRate::KilobitsPerSec(8)}}}};

  std::vector<VideoCodecStats::Frame> frames = {
      TestFrame().Fn(0).Ts(0).S(0).T(0).Es(es),
      TestFrame().Fn(0).Ts(0).S(1).T(0).Es(es),
      TestFrame().Fn(1).Ts(1).S(0).T(1).Es(es),
      TestFrame().Fn(1).Ts(1).S(1).T(1).Es(es)};

  std::vector<VideoCodecStats::Frame> sframes =
      VideoCodecStatsImpl::Merge(frames);
  ASSERT_THAT(sframes, SizeIs(2));
  ASSERT_TRUE(sframes[0].target_framerate.has_value());
  ASSERT_TRUE(sframes[1].target_framerate.has_value());
  ASSERT_TRUE(sframes[0].target_bitrate.has_value());
  ASSERT_TRUE(sframes[1].target_bitrate.has_value());
  EXPECT_EQ(*sframes[0].target_framerate, Frequency::Hertz(2));
  EXPECT_EQ(*sframes[1].target_framerate, Frequency::Hertz(2));
  EXPECT_EQ(*sframes[0].target_bitrate, DataRate::KilobitsPerSec(12));
  EXPECT_EQ(*sframes[1].target_bitrate, DataRate::KilobitsPerSec(12));
}

TEST(VideoCodecStatsImpl, AggregateBitrate) {
  VideoCodecStatsImpl stats;
  stats.AddFrame({.frame_num = 0,
                  .timestamp_rtp = 0,
                  .frame_size = DataSize::Bytes(1000),
                  .target_bitrate = DataRate::BytesPerSec(1000)});
  stats.AddFrame({.frame_num = 1,
                  .timestamp_rtp = 90000,
                  .frame_size = DataSize::Bytes(2000),
                  .target_bitrate = DataRate::BytesPerSec(1000)});

  Stream stream = stats.Aggregate();
  EXPECT_EQ(stream.encoded_bitrate_kbps.GetAverage(), 12.0);
  EXPECT_EQ(stream.bitrate_mismatch_pct.GetAverage(), 50.0);
}

TEST(VideoCodecStatsImpl, AggregateFramerate) {
  VideoCodecStatsImpl stats;
  stats.AddFrame({.frame_num = 0,
                  .timestamp_rtp = 0,
                  .frame_size = DataSize::Bytes(1),
                  .target_framerate = Frequency::Hertz(1)});
  stats.AddFrame({.frame_num = 1,
                  .timestamp_rtp = 90000,
                  .frame_size = DataSize::Zero(),
                  .target_framerate = Frequency::Hertz(1)});

  Stream stream = stats.Aggregate();
  EXPECT_EQ(stream.encoded_framerate_fps.GetAverage(), 0.5);
  EXPECT_EQ(stream.framerate_mismatch_pct.GetAverage(), -50.0);
}

TEST(VideoCodecStatsImpl, AggregateTransmissionTime) {
  VideoCodecStatsImpl stats;
  stats.AddFrame({.frame_num = 0,
                  .timestamp_rtp = 0,
                  .frame_size = DataSize::Bytes(2),
                  .target_bitrate = DataRate::BytesPerSec(1)});
  stats.AddFrame({.frame_num = 1,
                  .timestamp_rtp = 90000,
                  .frame_size = DataSize::Bytes(3),
                  .target_bitrate = DataRate::BytesPerSec(1)});

  Stream stream = stats.Aggregate();
  ASSERT_EQ(stream.transmission_time_ms.NumSamples(), 2);
  ASSERT_EQ(stream.transmission_time_ms.GetSamples()[0], 2000);
  ASSERT_EQ(stream.transmission_time_ms.GetSamples()[1], 4000);
}

}  // namespace test
}  // namespace webrtc
