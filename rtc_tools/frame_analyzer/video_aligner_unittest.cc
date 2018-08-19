/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This test doesn't actually verify the output since it's just printed
// to stdout by void functions, but it's still useful as it executes the code.

#include "rtc_tools/frame_analyzer/video_aligner.h"
#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/y4m_file_reader.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

class VideoAlignerTest : public ::testing::Test {
 protected:
  void SetUp() {
    reference_video =
        Y4mFile::Open(ResourcePath("reference_video_640x360_30fps", "y4m"));
    ASSERT_TRUE(reference_video);
  }

  rtc::scoped_refptr<Y4mFile> reference_video;
};

TEST_F(VideoAlignerTest, FindMatchingFrameIndiciesEmpty) {
  rtc::scoped_refptr<Video> empty_test_video =
      ReorderVideo(reference_video, std::vector<size_t>());

  const std::vector<size_t> matched_indicies =
      FindMatchingFrameIndicies(reference_video, empty_test_video);

  EXPECT_TRUE(matched_indicies.empty());
}

TEST_F(VideoAlignerTest, FindMatchingFrameIndiciesIdentity) {
  const std::vector<size_t> indicies =
      FindMatchingFrameIndicies(reference_video, reference_video);

  EXPECT_EQ(indicies.size(), reference_video->number_of_frames());
  for (size_t i = 0; i < indicies.size(); ++i)
    EXPECT_EQ(i, indicies[i]);
}

TEST_F(VideoAlignerTest, FindMatchingFrameIndiciesDuplicateFrames) {
  const std::vector<size_t> indicies = {2, 2, 2, 2};

  // Generate a test video based on this sequence.
  rtc::scoped_refptr<Video> test_video =
      ReorderVideo(reference_video, indicies);

  const std::vector<size_t> matched_indicies =
      FindMatchingFrameIndicies(reference_video, test_video);

  EXPECT_EQ(indicies, matched_indicies);
}

TEST_F(VideoAlignerTest, FindMatchingFrameIndiciesLoopAround) {
  std::vector<size_t> indicies;
  for (size_t i = 0; i < reference_video->number_of_frames() * 2; ++i)
    indicies.push_back(i % reference_video->number_of_frames());

  // Generate a test video based on this sequence.
  rtc::scoped_refptr<Video> test_video =
      ReorderVideo(reference_video, indicies);

  const std::vector<size_t> matched_indicies =
      FindMatchingFrameIndicies(reference_video, test_video);

  for (size_t i = 0; i < matched_indicies.size(); ++i)
    EXPECT_EQ(i, matched_indicies[i]);
}

TEST_F(VideoAlignerTest, FindMatchingFrameIndiciesStressTest) {
  std::vector<size_t> indicies;
  // Arbitrary start index.
  const size_t start_index = 12345;
  // Genereate some generic sequence of frames.
  indicies.push_back(start_index % reference_video->number_of_frames());
  indicies.push_back((start_index + 1) % reference_video->number_of_frames());
  indicies.push_back((start_index + 2) % reference_video->number_of_frames());
  indicies.push_back((start_index + 5) % reference_video->number_of_frames());
  indicies.push_back((start_index + 10) % reference_video->number_of_frames());
  indicies.push_back((start_index + 20) % reference_video->number_of_frames());
  indicies.push_back((start_index + 20) % reference_video->number_of_frames());
  indicies.push_back((start_index + 22) % reference_video->number_of_frames());
  indicies.push_back((start_index + 32) % reference_video->number_of_frames());

  // Generate a test video based on this sequence.
  rtc::scoped_refptr<Video> test_video =
      ReorderVideo(reference_video, indicies);

  const std::vector<size_t> matched_indicies =
      FindMatchingFrameIndicies(reference_video, test_video);

  EXPECT_EQ(indicies, matched_indicies);
}

TEST_F(VideoAlignerTest, GenerateAlignedReferenceVideo) {
  // Arbitrary start index.
  const size_t start_index = 12345;
  std::vector<size_t> indicies;
  const size_t frame_step = 10;
  for (size_t i = 0; i < reference_video->number_of_frames() / frame_step;
       ++i) {
    indicies.push_back((start_index + i * frame_step) %
                       reference_video->number_of_frames());
  }

  // Generate a test video based on this sequence.
  rtc::scoped_refptr<Video> test_video =
      ReorderVideo(reference_video, indicies);

  rtc::scoped_refptr<Video> aligned_reference_video =
      GenerateAlignedReferenceVideo(reference_video, test_video);

  // Assume perfect match, i.e. ssim == 1, for all frames.
  for (size_t i = 0; i < test_video->number_of_frames(); ++i) {
    EXPECT_EQ(1.0, Ssim(test_video->GetFrame(i),
                        aligned_reference_video->GetFrame(i)));
  }
}

}  // namespace test
}  // namespace webrtc
