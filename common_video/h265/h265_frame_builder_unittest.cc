/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_frame_builder.h"

#include "common_video/h265/h265_annexb_bitstream_builder.h"
#include "common_video/h265/h265_bitstream_parser.h"
#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_sps_parser.h"
#include "test/gtest.h"

namespace webrtc {

class H265FrameBuilderTest : public ::testing::Test {
 public:
  H265FrameBuilderTest() {}
  ~H265FrameBuilderTest() override {}
};

TEST_F(H265FrameBuilderTest, TestBuildKeyFrameSingleTemporalLayer) {
  H265AnnexBBitstreamBuilder builder(
      /*insert_emulation_prevention_bytes=*/true);

  BuildKeyFrame(builder, 1920, 1080, /*num_temporal_layers=*/1,
                /*qp=*/30, /*frame_size_bytes=*/2000);
  H265BitstreamParser h265_parser;
  h265_parser.ParseBitstream(builder.data());

  std::optional<int> qp = h265_parser.GetLastSliceQp();
  ASSERT_TRUE(qp.has_value());
  EXPECT_EQ(30, *qp);

  const H265SpsParser::SpsState* sps = h265_parser.GetSPS(0);
  ASSERT_EQ(sps->width, 1920u);
  ASSERT_EQ(sps->height, 1080u);
  ASSERT_EQ(sps->sps_max_sub_layers_minus1, 0u);

  BuildDeltaFrame(builder, /*temporal_layer_id=*/1, /*qp=*/27,
                  /*frame_size_bytes=*/2000, /*wrapped_on_16_poc_lsb=*/1);
  h265_parser.ParseBitstream(builder.data());
  qp = h265_parser.GetLastSliceQp();
  ASSERT_TRUE(qp.has_value());
  EXPECT_EQ(27, *qp);
}

TEST_F(H265FrameBuilderTest, TestBuildFramesTwoTemporalLayers) {
  H265AnnexBBitstreamBuilder builder(
      /*insert_emulation_prevention_bytes=*/true);

  BuildKeyFrame(builder, 1280, 720, /*num_temporal_layers=*/2, /*qp=*/30,
                /*frame_size_bytes=*/2000);
  H265BitstreamParser h265_parser;
  h265_parser.ParseBitstream(builder.data());

  std::optional<int> qp = h265_parser.GetLastSliceQp();
  ASSERT_TRUE(qp.has_value());
  EXPECT_EQ(30, *qp);

  const H265SpsParser::SpsState* sps = h265_parser.GetSPS(0);
  ASSERT_EQ(sps->width, 1280u);
  ASSERT_EQ(sps->height, 720u);
  ASSERT_EQ(sps->sps_max_sub_layers_minus1, 1u);

  constexpr uint8_t kDeltaFrames = 32;
  for (int i = 1; i <= kDeltaFrames; ++i) {
    BuildDeltaFrame(builder, (i % 2), 27 + 3 * (i % 2), 2000, (i + 1) % 16);
    h265_parser.ParseBitstream(builder.data());
    qp = h265_parser.GetLastSliceQp();
    ASSERT_TRUE(qp.has_value());
    EXPECT_EQ(27 + 3 * (i % 2), *qp);
  }
}

TEST_F(H265FrameBuilderTest, TestBuildFramesThreeTemporalLayers) {
  H265AnnexBBitstreamBuilder builder(
      /*insert_emulation_prevention_bytes=*/true);

  BuildKeyFrame(builder, 1280, 720, /*num_temporal_layers=*/3, /*qp=*/30,
                /*frame_size_bytes=*/2000);
  H265BitstreamParser h265_parser;
  h265_parser.ParseBitstream(builder.data());

  std::optional<int> qp = h265_parser.GetLastSliceQp();
  ASSERT_TRUE(qp.has_value());
  EXPECT_EQ(30, *qp);

  const H265SpsParser::SpsState* sps = h265_parser.GetSPS(0);
  ASSERT_EQ(sps->width, 1280u);
  ASSERT_EQ(sps->height, 720u);
  ASSERT_EQ(sps->sps_max_sub_layers_minus1, 2u);

  constexpr uint8_t kDeltaFrames = 32;
  for (int i = 1; i <= kDeltaFrames; ++i) {
    BuildDeltaFrame(builder, (i % 3), 33 + 3 * (i % 3), 2000, (i + 1) % 16);
    h265_parser.ParseBitstream(builder.data());
    qp = h265_parser.GetLastSliceQp();
    ASSERT_TRUE(qp.has_value());
    EXPECT_EQ(33 + 3 * (i % 3), *qp);
  }
}

}  // namespace webrtc
