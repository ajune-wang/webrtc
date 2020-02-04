/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/encoder_buffers_converter.h"

#include "api/video/video_frame_type.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

constexpr CodecBufferUsage kReferenceAndUpdate(0,
                                               /*referenced=*/true,
                                               /*updated=*/true);
constexpr CodecBufferUsage kReference(0,
                                      /*referenced=*/true,
                                      /*updated=*/false);
constexpr CodecBufferUsage kUpdate(0, /*referenced=*/false, /*updated=*/true);
constexpr CodecBufferUsage kNone(0, /*referenced=*/false, /*updated=*/false);

TEST(EncoderBuffersConverterTest, SingleLayer) {
  CodecBufferUsage pattern[] = {kReferenceAndUpdate};
  EncoderBuffersConverter converter;

  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameKey,
                                              /*frame_id=*/1, pattern),
              IsEmpty());
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/3, pattern),
              ElementsAre(1));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/6, pattern),
              ElementsAre(3));
}

TEST(EncoderBuffersConverterTest, TwoTemporalLayers) {
  // Shortened 4-frame pattern:
  // T1:  2---4   6---8 ...
  //      /   /   /   /
  // T0: 1---3---5---7 ...
  CodecBufferUsage pattern[][3] = {{kReferenceAndUpdate, kNone, kNone},
                                   {kReference, kUpdate, kNone},
                                   {kReferenceAndUpdate, kNone, kNone},
                                   {kReference, kReference, kNone}};
  EncoderBuffersConverter converter;

  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameKey,
                                              /*frame_id=*/1, pattern[0]),
              IsEmpty());
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/2, pattern[1]),
              ElementsAre(1));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/3, pattern[2]),
              ElementsAre(1));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/4, pattern[3]),
              UnorderedElementsAre(2, 3));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/5, pattern[0]),
              ElementsAre(3));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/6, pattern[1]),
              ElementsAre(5));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/7, pattern[2]),
              ElementsAre(5));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/8, pattern[3]),
              UnorderedElementsAre(6, 7));
}

TEST(EncoderBuffersConverterTest, ThreeTemporalLayers4FramePattern) {
  // T2:   2---4   6---8 ...
  //      /   /   /   /
  // T1:  |  3    |  7   ...
  //      /_/     /_/
  // T0: 1-------5-----  ...
  CodecBufferUsage pattern[][3] = {{kReferenceAndUpdate, kNone, kNone},
                                   {kReference, kNone, kUpdate},
                                   {kReference, kUpdate, kNone},
                                   {kReference, kReference, kReference}};
  EncoderBuffersConverter converter;

  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameKey,
                                              /*frame_id=*/1, pattern[0]),
              IsEmpty());
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/2, pattern[1]),
              ElementsAre(1));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/3, pattern[2]),
              ElementsAre(1));
  // Note that frame#4 references buffer#0 that is updated by frame#1,
  // yet there is no direct dependency from frame#4 to frame#1.
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/4, pattern[3]),
              UnorderedElementsAre(2, 3));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/5, pattern[0]),
              ElementsAre(1));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/6, pattern[1]),
              ElementsAre(5));
}

TEST(EncoderBuffersConverterTest, SimulcastWith2Layers) {
  // S1: 2---4---6-  ...
  //
  // S0: 1---3---5-  ...
  CodecBufferUsage pattern[][2] = {
      {kReferenceAndUpdate, kNone},
      {kNone, kReferenceAndUpdate},
  };
  EncoderBuffersConverter converter;

  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameKey,
                                              /*frame_id=*/1, pattern[0]),
              IsEmpty());
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameKey,
                                              /*frame_id=*/2, pattern[1]),
              IsEmpty());
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/3, pattern[0]),
              ElementsAre(1));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/4, pattern[1]),
              ElementsAre(2));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/5, pattern[0]),
              ElementsAre(3));
  EXPECT_THAT(converter.CalculateDependencies(VideoFrameType::kVideoFrameDelta,
                                              /*frame_id=*/6, pattern[1]),
              ElementsAre(4));
}

}  // namespace
}  // namespace webrtc
