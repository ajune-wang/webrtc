/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_layers_allocation_extension.h"
#include "api/video/video_layers_allocation.h"

#include "test/gmock.h"

namespace webrtc {
namespace {

void VerifyEquals(const VideoLayersAllocation& lhs,
                  const VideoLayersAllocation& rhs) {
  EXPECT_EQ(lhs.simulcast_id, rhs.simulcast_id);
  EXPECT_EQ(lhs.resolution_and_frame_rate, rhs.resolution_and_frame_rate);
  for (int i = 0; i < VideoLayersAllocation::kMaxSpatialIds; ++i) {
    EXPECT_EQ(lhs.target_bitrate[i], rhs.target_bitrate[i]);
  }
  EXPECT_TRUE(lhs.Equals(rhs));
}

TEST(RtpVideoLayersAllocationExtension,
     WriteEmptyLayersAllocationReturnsFalse) {
  VideoLayersAllocation written_allocation;
  uint8_t buffer[20];
  EXPECT_FALSE(
      RtpVideoLayersAllocationExtension::Write(buffer, written_allocation));
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParse2SpatialWith2TemporalLayers) {
  VideoLayersAllocation written_allocation;
  written_allocation.simulcast_id = 1;
  written_allocation.target_bitrate[0] = {25000, 50000};
  written_allocation.target_bitrate[1] = {100000, 200000};

  uint8_t buffer[10];
  RtpVideoLayersAllocationExtension::Write(buffer, written_allocation);
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  VerifyEquals(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseAllocationWithDifferentNumerOfTemporalLayers) {
  VideoLayersAllocation written_allocation;
  written_allocation.simulcast_id = 1;
  written_allocation.target_bitrate[0] = {25000, 50000};
  written_allocation.target_bitrate[1] = {100000};

  uint8_t buffer[10];
  RtpVideoLayersAllocationExtension::Write(buffer, written_allocation);
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  VerifyEquals(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension,
     CanWriteAndParseAllocationWithMixedHighAndLowBitrate) {
  VideoLayersAllocation written_allocation;
  written_allocation.simulcast_id = 0;
  written_allocation.target_bitrate[0] = {25000, 999000000, 6000};

  uint8_t buffer[10];
  RtpVideoLayersAllocationExtension::Write(buffer, written_allocation);
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  VerifyEquals(written_allocation, parsed_allocation);
}

TEST(RtpVideoLayersAllocationExtension, CanWriteAndParseFullData) {
  VideoLayersAllocation written_allocation;
  written_allocation.simulcast_id = 1;
  written_allocation.target_bitrate[0] = {25000, 50000};
  written_allocation.target_bitrate[1] = {100000, 200000};

  written_allocation.resolution_and_frame_rate = {
      {.width = 640, .height = 360, .frame_rate = 30},
      {.width = 320, .height = 160, .frame_rate = 30}};

  uint8_t buffer[20];
  RtpVideoLayersAllocationExtension::Write(buffer, written_allocation);
  VideoLayersAllocation parsed_allocation;
  EXPECT_TRUE(
      RtpVideoLayersAllocationExtension::Parse(buffer, &parsed_allocation));
  VerifyEquals(written_allocation, parsed_allocation);
}

}  // namespace
}  // namespace webrtc
