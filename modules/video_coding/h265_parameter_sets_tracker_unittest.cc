/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h265_parameter_sets_tracker.h"

#include <string.h>

#include <vector>

#include "absl/types/variant.h"
#include "common_video/h265/h265_common.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {
namespace {

// VPS/SPS/PPS/IDR for a 1280x720 camera capture from ffmpeg on linux.
// Contains emulation bytes but no cropping. This buffer is generated with
// following command: 1) ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 1280x720
// camera.h265
//
// The VPS/SPS/PPS are kept intact while idr1/idr2/cra1/cra2/trail1/trail2 are
// created by changing the NALU type of original IDR/TRAIL_R NALUs, and
// truncated only for testing of the tracker.
uint8_t vps[] = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff,
                 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03,
                 0x00, 0x00, 0x03, 0x00, 0x5d, 0x95, 0x98, 0x09};
uint8_t sps[] = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x60,
                 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00,
                 0x00, 0x03, 0x00, 0x5d, 0xa0, 0x02, 0x80, 0x80, 0x2d,
                 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0xc0, 0x5a, 0x70,
                 0x80, 0x00, 0x01, 0xf4, 0x80, 0x00, 0x3a, 0x98, 0x04};
uint8_t pps[] = {0x00, 0x00, 0x00, 0x01, 0x44, 0x01,
                 0xc1, 0x72, 0xb4, 0x62, 0x40};
uint8_t aud_key_frame[] = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x10};
uint8_t idr1[] = {0x00, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf,
                  0x08, 0x46, 0x0c, 0x92, 0xa3, 0xf4, 0x77};
uint8_t idr2[] = {0x00, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf,
                  0x08, 0x46, 0x0c, 0x92, 0xa3, 0xf4, 0x77};
uint8_t trail1[] = {0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0xa4, 0x04, 0x55,
                    0xa2, 0x6d, 0xce, 0xc0, 0xc3, 0xed, 0x0b, 0xac, 0xbc,
                    0x00, 0xc4, 0x44, 0x2e, 0xf7, 0x55, 0xfd, 0x05, 0x86};
uint8_t trail2[] = {0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x23, 0xfc, 0x20,
                    0x22, 0xad, 0x13, 0x68, 0xce, 0xc3, 0x5a, 0x00, 0x01,
                    0x80, 0xe9, 0xc6, 0x38, 0x13, 0xec, 0xef, 0x0f, 0xff};
uint8_t cra[] = {0x00, 0x00, 0x00, 0x01, 0x2A, 0x01, 0xad, 0x00, 0x58, 0x81,
                 0x04, 0x11, 0xc2, 0x00, 0x44, 0x3f, 0x34, 0x46, 0x3e, 0xcc,
                 0x86, 0xd9, 0x3f, 0xf1, 0xe1, 0xda, 0x26, 0xb1, 0xc5, 0x50,
                 0xf2, 0x8b, 0x8d, 0x0c, 0xe9, 0xe1, 0xd3, 0xe0, 0xa7, 0x3e};
uint8_t aud_delta_frame[] = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x30};

// Below two H264 binaries are copied from h264 bitstream parser unittests,
// to check the behavior of the tracker on stream from missmatched encoder.
uint8_t sps_pps_h264[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda,
                          0x01, 0x40, 0x16, 0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00,
                          0x00, 0x00, 0x01, 0x68, 0xce, 0x06, 0xe2};
uint8_t idr_h264[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda, 0x01, 0x40, 0x16,
    0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x06,
    0xe2, 0x00, 0x00, 0x00, 0x01, 0x65, 0xb8, 0x40, 0xf0, 0x8c, 0x03, 0xf2,
    0x75, 0x67, 0xad, 0x41, 0x64, 0x24, 0x0e, 0xa0, 0xb2, 0x12, 0x1e, 0xf8,
};

using ::testing::ElementsAreArray;

rtc::ArrayView<const uint8_t> Bitstream(
    const H265ParameterSetsTracker::FixedBitstream& fixed) {
  return fixed.bitstream;
}

}  // namespace

class TestH265ParameterSetsTracker : public ::testing::Test {
 public:
  H265ParameterSetsTracker tracker_;
};

TEST_F(TestH265ParameterSetsTracker, NoNalus) {
  uint8_t data[] = {1, 2, 3};

  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);
}

TEST_F(TestH265ParameterSetsTracker, StreamFromMissMatchingH26xCodec) {
  std::vector<uint8_t> data;
  unsigned sps_pps_size = sizeof(sps_pps_h264) / sizeof(sps_pps_h264[0]);
  unsigned idr_size = sizeof(idr_h264) / sizeof(idr_h264[0]);
  data.insert(data.end(), sps_pps_h264, sps_pps_h264 + sps_pps_size);
  data.insert(data.end(), idr_h264, idr_h264 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  // This is not an H.265 stream. We simply pass through it.
  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);
}

TEST_F(TestH265ParameterSetsTracker, AllParameterSetsInCurrentIdrSingleSlice) {
  std::vector<uint8_t> data;
  data.clear();
  unsigned vps_size = sizeof(vps) / sizeof(uint8_t);
  unsigned sps_size = sizeof(sps) / sizeof(uint8_t);
  unsigned pps_size = sizeof(pps) / sizeof(uint8_t);
  unsigned idr_size = sizeof(idr1) / sizeof(uint8_t);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size - 1);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);
}

TEST_F(TestH265ParameterSetsTracker, AllParameterSetsMissingForIdr) {
  std::vector<uint8_t> data;
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kRequestKeyframe);
}

TEST_F(TestH265ParameterSetsTracker, VpsMissingForIdr) {
  std::vector<uint8_t> data;
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kRequestKeyframe);
}

TEST_F(TestH265ParameterSetsTracker,
       ParameterSetsSeenBeforeButRepeatedVpsMissingForCurrentIdr) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);

  // Second IDR but encoder only repeats SPS/PPS(unlikely to happen).
  std::vector<uint8_t> frame2;
  unsigned sps2_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps2_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  frame2.insert(frame2.end(), sps, sps + sps2_size);
  frame2.insert(frame2.end(), pps, pps + pps2_size);
  frame2.insert(frame2.end(), idr2, idr2 + idr2_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  // If any of the parameter set is missing, we append all of VPS/SPS/PPS and it
  // is fine to repeat any of the parameter set twice for current IDR.
  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kInsert);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), idr2, idr2 + idr2_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH265ParameterSetsTracker,
       AllParameterSetsInCurrentIdrMulitpleSlices) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr1_size = sizeof(idr1) / sizeof(idr1[0]);
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr1_size);
  data.insert(data.end(), idr2, idr2 + idr2_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);
}

TEST_F(TestH265ParameterSetsTracker,
       SingleDeltaSliceNoAudWithoutParameterSetsBefore) {
  std::vector<uint8_t> data;
  unsigned trail_size = sizeof(trail1) / sizeof(trail1[0]);
  data.insert(data.end(), trail1, trail1 + trail_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  // We won't attempt to fix delta frames without aud, so they'll be passed
  // through.
  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);
}

TEST_F(TestH265ParameterSetsTracker,
       MultipleDeltaSlicseNoAudWithoutParameterSetsBefore) {
  std::vector<uint8_t> data;
  unsigned trail1_size = sizeof(trail1) / sizeof(trail1[0]);
  unsigned trail2_size = sizeof(trail2) / sizeof(trail2[0]);
  data.insert(data.end(), trail1, trail1 + trail1_size);
  data.insert(data.end(), trail2, trail2 + trail2_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  // We won't attempt to fix delta frames without aud, so they'll be passed
  // through.
  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);
}

TEST_F(TestH265ParameterSetsTracker,
       SingleDeltaSliceWithAudWithoutParameterSetsBefore) {
  std::vector<uint8_t> data;
  unsigned delta_aud_size =
      sizeof(aud_delta_frame) / sizeof(aud_delta_frame[0]);
  unsigned trail_size = sizeof(trail1) / sizeof(trail1[0]);
  data.insert(data.end(), aud_delta_frame, aud_delta_frame + delta_aud_size);
  data.insert(data.end(), trail1, trail1 + trail_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  // We won't attempt to fix delta frames without aud, so they'll be passed
  // through.
  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kDropAud);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), trail1, trail1 + trail_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH265ParameterSetsTracker,
       ParameterSetsInPreviousIdrNotInCurrentIdr) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);

  std::vector<uint8_t> frame2;
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  frame2.insert(frame2.end(), idr2, idr2 + idr2_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kInsert);

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), idr2, idr2 + idr2_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH265ParameterSetsTracker,
       ParameterSetsInPreviousIdrNotInCurrentIdrAndFramesAreAllWithAuds) {
  std::vector<uint8_t> data;
  unsigned key_aud_size = sizeof(aud_key_frame) / sizeof(aud_key_frame[0]);
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr1_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), aud_key_frame, aud_key_frame + key_aud_size);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr1_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kDropAud);

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), idr1, idr1 + idr1_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));

  std::vector<uint8_t> frame2;
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  frame2.insert(frame2.end(), aud_key_frame, aud_key_frame + key_aud_size);
  frame2.insert(frame2.end(), idr2, idr2 + idr2_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kInsertAndDropAud);
  std::vector<uint8_t> expected2;
  expected2.insert(expected2.end(), vps, vps + vps_size);
  expected2.insert(expected2.end(), sps, sps + sps_size);
  expected2.insert(expected2.end(), pps, pps + pps_size);
  expected2.insert(expected2.end(), idr2, idr2 + idr2_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected2));
}

TEST_F(TestH265ParameterSetsTracker,
       ParameterSetsInPreviousIdrNotInCurrentCra) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);

  std::vector<uint8_t> frame2;
  unsigned cra_size = sizeof(cra) / sizeof(cra[0]);
  frame2.insert(frame2.end(), cra, cra + cra_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kInsert);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), cra, cra + cra_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH265ParameterSetsTracker, ParameterSetsInBothPreviousAndCurrentIdr) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);

  std::vector<uint8_t> frame2;
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  frame2.insert(frame2.end(), vps, vps + vps_size);
  frame2.insert(frame2.end(), sps, sps + sps_size);
  frame2.insert(frame2.end(), pps, pps + pps_size);
  frame2.insert(frame2.end(), idr2, idr2 + idr2_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);
}

TEST_F(TestH265ParameterSetsTracker,
       AllParameterSetsInCurrentIdrSingleSliceWithAud) {
  std::vector<uint8_t> data;
  unsigned key_aud_size = sizeof(aud_key_frame) / sizeof(aud_key_frame[0]);
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), aud_key_frame, aud_key_frame + key_aud_size);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kDropAud);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), idr1, idr1 + idr_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH265ParameterSetsTracker,
       AllParameterSetsInCurrentIdrMultipleSlicesWithAud) {
  std::vector<uint8_t> data;
  unsigned key_aud_size = sizeof(aud_key_frame) / sizeof(aud_key_frame[0]);
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr1_size = sizeof(idr1) / sizeof(idr1[0]);
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  data.insert(data.end(), aud_key_frame, aud_key_frame + key_aud_size);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr1_size);
  data.insert(data.end(), aud_key_frame, aud_key_frame + key_aud_size);
  data.insert(data.end(), idr2, idr2 + idr2_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kDropAud);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), idr1, idr1 + idr1_size);
  expected.insert(expected.end(), aud_key_frame, aud_key_frame + key_aud_size);
  expected.insert(expected.end(), idr2, idr2 + idr2_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(TestH265ParameterSetsTracker, TwoGopsWithIdrTrailAndCra) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);

  // Second frame, a TRAIL_R picture.
  std::vector<uint8_t> frame2;
  unsigned trail_size = sizeof(trail1) / sizeof(trail1[0]);
  frame2.insert(frame2.end(), trail1, trail1 + trail_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);

  // Third frame, a TRAIL_R picture.
  std::vector<uint8_t> frame3;
  unsigned trail2_size = sizeof(trail2) / sizeof(trail2[0]);
  frame3.insert(frame3.end(), trail2, trail2 + trail2_size);
  fixed = tracker_.MaybeFixBitstream(frame3);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);

  // Fourth frame, a CRA picture.
  std::vector<uint8_t> frame4;
  unsigned cra_size = sizeof(cra) / sizeof(cra[0]);
  frame4.insert(frame4.end(), cra, cra + cra_size);
  fixed = tracker_.MaybeFixBitstream(frame4);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kInsert);

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), cra, cra + cra_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));

  // Last frame, a TRAIL_R picture with 2 slices.
  std::vector<uint8_t> frame5;
  unsigned trail3_size = sizeof(trail1) / sizeof(trail1[0]);
  unsigned trail4_size = sizeof(trail2) / sizeof(trail2[0]);
  frame5.insert(frame5.end(), trail1, trail1 + trail3_size);
  frame5.insert(frame5.end(), trail2, trail2 + trail4_size);
  fixed = tracker_.MaybeFixBitstream(frame5);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::kPassThrough);
}

}  // namespace video_coding
}  // namespace webrtc
