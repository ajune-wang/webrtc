/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/video_frame_stash.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(VideoFrameStashTest, SimpleConstructionValid) {
  constexpr size_t capacity = 128;
  video_coding::VideoFrameStash video_frame_stash(capacity);
  EXPECT_EQ(video_frame_stash.GetCapacity(), capacity);
  EXPECT_EQ(video_frame_stash.size(), size_t{0});
}

TEST(VideoFrameStashTest, SimpleInsertion) {
  constexpr size_t capacity = 128;
  video_coding::VideoFrameStash video_frame_stash(capacity);
  EXPECT_EQ(video_frame_stash.size(), size_t{0});
  for (size_t insert_count = 1; insert_count <= capacity; insert_count++) {
    video_frame_stash.StashFrame(nullptr);
    EXPECT_EQ(video_frame_stash.size(), insert_count);
    EXPECT_EQ(video_frame_stash.GetCapacity(), capacity);
  }
}

TEST(VideoFrameStashTest, SimpleRemove) {
  constexpr size_t capacity = 128;
  video_coding::VideoFrameStash video_frame_stash(capacity);
  video_frame_stash.StashFrame(nullptr);
  EXPECT_EQ(video_frame_stash.size(), size_t{1});
  video_frame_stash.RemoveFrame(video_frame_stash.begin());
  EXPECT_EQ(video_frame_stash.size(), size_t{0});
}

TEST(VideoFrameStashTest, OverCapacityInsert) {
  constexpr size_t capacity = 128;
  video_coding::VideoFrameStash video_frame_stash(capacity);
  EXPECT_EQ(video_frame_stash.size(), size_t{0});
  for (size_t insert_count = 1; insert_count <= capacity; insert_count++) {
    video_frame_stash.StashFrame(nullptr);
    EXPECT_EQ(video_frame_stash.size(), insert_count);
    EXPECT_EQ(video_frame_stash.GetCapacity(), capacity);
  }
  // On the second run the values are the same as the capacity.
  for (size_t insert_count = 1; insert_count <= capacity; insert_count++) {
    video_frame_stash.StashFrame(nullptr);
    EXPECT_EQ(video_frame_stash.size(), capacity);
    EXPECT_EQ(video_frame_stash.GetCapacity(), capacity);
  }
}

TEST(VideoFrameStashTest, RemoveAllElements) {
  constexpr size_t capacity = 128;
  video_coding::VideoFrameStash video_frame_stash(capacity);
  EXPECT_EQ(video_frame_stash.size(), size_t{0});
  for (size_t insert_count = 1; insert_count <= capacity; insert_count++) {
    video_frame_stash.StashFrame(nullptr);
    EXPECT_EQ(video_frame_stash.size(), insert_count);
    EXPECT_EQ(video_frame_stash.GetCapacity(), capacity);
  }
  // On the second run the values are the same as the capacity.
  auto remove_it = video_frame_stash.begin();
  for (size_t remove_count = 1; remove_count <= capacity; remove_count++) {
    remove_it = video_frame_stash.RemoveFrame(remove_it);
    EXPECT_EQ(video_frame_stash.size(), capacity - remove_count);
    EXPECT_EQ(video_frame_stash.GetCapacity(), capacity);
  }
  EXPECT_EQ(video_frame_stash.size(), size_t{0});
}

}  // namespace test.
}  // namespace webrtc.
