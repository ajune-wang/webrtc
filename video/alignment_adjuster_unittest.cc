/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/alignment_adjuster.h"

#include <memory>
#include <vector>

#include "rtc_base/numerics/safe_conversions.h"
#include "test/encoder_settings.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
constexpr int kRequestedAlignment = 2;

VideoEncoder::EncoderInfo GetEncoderInfo(int alignment, bool apply) {
  VideoEncoder::EncoderInfo info;
  info.requested_resolution_alignment = alignment;
  info.apply_alignment_to_all_simulcast_layers = apply;
  return info;
}

}  // namespace

class AlignmentAdjusterTest
    : public ::testing::TestWithParam<
          ::testing::tuple<std::vector<double>, std::vector<double>, int> > {};

INSTANTIATE_TEST_SUITE_P(
    ScaleFactorsAndAlignment,
    AlignmentAdjusterTest,
    // std::make_tuple(
    // input: scale factors,
    // output: adjusted scale factors,
    // output: adjusted alignment)
    ::testing::Values(
        std::make_tuple(std::vector<double>{-1.0},
                        std::vector<double>{-1.0},
                        kRequestedAlignment * 1),  // default: 1.0
        std::make_tuple(std::vector<double>{-1.0, -1.0},
                        std::vector<double>{-1.0, -1.0},
                        kRequestedAlignment * 2),  // default: 1.0, 2.0
        std::make_tuple(std::vector<double>{-1.0, -1.0, -1.0},
                        std::vector<double>{-1.0, -1.0, -1.0},
                        kRequestedAlignment * 4),  // default: 1.0, 2.0, 4.0
        std::make_tuple(std::vector<double>{1.0, 2.0, 4.0},
                        std::vector<double>{1.0, 2.0, 4.0},
                        kRequestedAlignment * 4),
        std::make_tuple(std::vector<double>{9999.0, -1.0, 1.0},
                        std::vector<double>{8.0, 1.0, 1.0},
                        kRequestedAlignment * 8),  // kMaxAlignment
        std::make_tuple(std::vector<double>{3.99, 2.01, 1.0},
                        std::vector<double>{4.0, 2.0, 1.0},
                        kRequestedAlignment * 4),
        std::make_tuple(std::vector<double>{2.9, 2.1},
                        std::vector<double>{6.0 / 2.0, 6.0 / 3.0},
                        kRequestedAlignment * 6),
        std::make_tuple(std::vector<double>{4.9, 1.7, 1.2},
                        std::vector<double>{5.0, 5.0 / 3.0, 5.0 / 4.0},
                        kRequestedAlignment * 5),
        std::make_tuple(std::vector<double>{1.0, 1.3},
                        std::vector<double>{4.0 / 4.0, 4.0 / 3.0},
                        kRequestedAlignment * 4),
        std::make_tuple(std::vector<double>{1.75, 3.5},
                        std::vector<double>{7.0 / 4.0, 7.0 / 2.0},
                        7),
        std::make_tuple(std::vector<double>{1.5, 2.5},
                        std::vector<double>{5.0 / 3.0, 5.0 / 2.0},
                        kRequestedAlignment * 5)));

TEST_P(AlignmentAdjusterTest, AlignmentAppliedToAllLayers) {
  const bool kApplyAlignmentToAllLayers = true;
  const std::vector<double> kScaleFactors = std::get<0>(GetParam());
  const std::vector<double> kAdjustedScaleFactors = std::get<1>(GetParam());
  const int kAdjustedAlignment = std::get<2>(GetParam());

  // Fill config with the scaling factor by which to reduce encoding size.
  const int num_streams = kScaleFactors.size();
  VideoEncoderConfig config;
  test::FillEncoderConfiguration(kVideoCodecVP8, num_streams, &config);
  for (int i = 0; i < num_streams; ++i) {
    config.simulcast_layers[i].scale_resolution_down_by = kScaleFactors[i];
  }

  // Verify requested alignment from sink.
  VideoEncoder::EncoderInfo info =
      GetEncoderInfo(kRequestedAlignment, kApplyAlignmentToAllLayers);
  int alignment =
      AlignmentAdjuster::GetAlignmentAndMaybeAdjustScaleFactors(info, &config);
  EXPECT_EQ(alignment, kAdjustedAlignment);

  // Verify adjusted scale factors.
  for (int i = 0; i < num_streams; ++i) {
    EXPECT_EQ(config.simulcast_layers[i].scale_resolution_down_by,
              kAdjustedScaleFactors[i]);
  }
}

TEST_P(AlignmentAdjusterTest, AlignmentNotAppliedToAllLayers) {
  const bool kApplyAlignmentToAllLayers = false;
  const std::vector<double> kScaleFactors = std::get<0>(GetParam());
  const std::vector<double> kAdjustedScaleFactors = std::get<1>(GetParam());

  // Fill config with the scaling factor by which to reduce encoding size.
  const int num_streams = kScaleFactors.size();
  VideoEncoderConfig config;
  test::FillEncoderConfiguration(kVideoCodecVP8, num_streams, &config);
  for (int i = 0; i < num_streams; ++i) {
    config.simulcast_layers[i].scale_resolution_down_by = kScaleFactors[i];
  }

  // Verify requested alignment from sink.
  VideoEncoder::EncoderInfo info =
      GetEncoderInfo(kRequestedAlignment, kApplyAlignmentToAllLayers);
  int alignment =
      AlignmentAdjuster::GetAlignmentAndMaybeAdjustScaleFactors(info, &config);
  EXPECT_EQ(alignment, kRequestedAlignment);

  // Verify that scale factors are not adjusted.
  for (int i = 0; i < num_streams; ++i) {
    EXPECT_EQ(config.simulcast_layers[i].scale_resolution_down_by,
              kScaleFactors[i]);
  }
}

}  // namespace test
}  // namespace webrtc
