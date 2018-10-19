/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/video_file_reader.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace test {

namespace {

const GeometryTransformationMatrix kIdentityGeometryMatrix = {
    {{1, 0, 0}, {0, 1, 0}}};

// void ExpectNear(const GeometryTransformationMatrix& expected,
//                 const GeometryTransformationMatrix& actual) {
//   const float kAbsErrorOffset = 0.1;
//   const float kAbsErrorXy = 1.0e-3;
//   EXPECT_NEAR(expected[0][0], actual[0][0], kAbsErrorXy);
//   EXPECT_NEAR(expected[0][1], actual[0][1], kAbsErrorXy);
//   EXPECT_NEAR(expected[0][3], actual[0][2], kAbsErrorOffset);

//   EXPECT_NEAR(expected[1][0], actual[1][0], kAbsErrorXy);
//   EXPECT_NEAR(expected[1][1], actual[1][1], kAbsErrorXy);
//   EXPECT_NEAR(expected[1][3], actual[1][2], kAbsErrorOffset);

//   EXPECT_NEAR(expected[2][0], actual[2][0], kAbsErrorXy);
//   EXPECT_NEAR(expected[2][1], actual[2][1], kAbsErrorXy);
//   EXPECT_NEAR(expected[2][3], actual[2][2], kAbsErrorOffset);
// }

}  // namespace

class VideoGeometryAlignerTest : public ::testing::Test {
 protected:
  void SetUp() {
    reference_video_ =
        OpenYuvFile(ResourcePath("foreman_128x96", "yuv"), 128, 96);
    ASSERT_TRUE(reference_video_);
  }

  rtc::scoped_refptr<Video> reference_video_;
};

TEST_F(VideoGeometryAlignerTest, AdjustGeometryFrameIdentity) {
  const rtc::scoped_refptr<I420BufferInterface> test_frame =
      reference_video_->GetFrame(0);

  // Assume perfect match, i.e. ssim == 1.
  EXPECT_EQ(1.0, Ssim(test_frame,
                      AdjustGeometry(kIdentityGeometryMatrix, test_frame)));
}

TEST_F(VideoGeometryAlignerTest,
       CalculateGeometryTransformationMatrixIdentity) {
  EXPECT_EQ(kIdentityGeometryMatrix, CalculateGeometryTransformationMatrix(
                                         reference_video_, reference_video_));
}

TEST_F(VideoGeometryAlignerTest,
       CalculateGeometryTransformationMatrixArbitrary) {
  // Arbitrary geometry transformation matrix.
  const GeometryTransformationMatrix org_geometry_matrix = {
      {{1, 0.0, 0}, {0.0, 95.0 / 96.0, 0}}};

  const GeometryTransformationMatrix result_geometry_matrix =
      CalculateGeometryTransformationMatrix(
          AdjustGeometry(org_geometry_matrix, reference_video_),
          reference_video_);

  EXPECT_EQ(org_geometry_matrix, result_geometry_matrix);
  // ExpectNear(org_geometry_matrix, result_geometry_matrix);
}

}  // namespace test
}  // namespace webrtc
