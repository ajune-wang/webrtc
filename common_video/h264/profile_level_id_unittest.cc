/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/profile_level_id.h"

#include <map>
#include <string>

#include "absl/types/optional.h"
#include "api/video_codecs/h264_profile_level_id.h"
#include "test/gtest.h"

namespace webrtc {

TEST(H264ProfileLevelId, TestParsingInvalid) {
  // Malformed strings.
  EXPECT_FALSE(ParseH264ProfileLevelId(""));
  EXPECT_FALSE(ParseH264ProfileLevelId(" 42e01f"));
  EXPECT_FALSE(ParseH264ProfileLevelId("4242e01f"));
  EXPECT_FALSE(ParseH264ProfileLevelId("e01f"));
  EXPECT_FALSE(ParseH264ProfileLevelId("gggggg"));

  // Invalid level.
  EXPECT_FALSE(ParseH264ProfileLevelId("42e000"));
  EXPECT_FALSE(ParseH264ProfileLevelId("42e00f"));
  EXPECT_FALSE(ParseH264ProfileLevelId("42e0ff"));

  // Invalid profile.
  EXPECT_FALSE(ParseH264ProfileLevelId("42e11f"));
  EXPECT_FALSE(ParseH264ProfileLevelId("58601f"));
  EXPECT_FALSE(ParseH264ProfileLevelId("64e01f"));
}

TEST(H264ProfileLevelId, TestParsingLevel) {
  EXPECT_EQ(H264Level::kLevel3_1, ParseH264ProfileLevelId("42e01f")->level);
  EXPECT_EQ(H264Level::kLevel1_1, ParseH264ProfileLevelId("42e00b")->level);
  EXPECT_EQ(H264Level::kLevel1_b, ParseH264ProfileLevelId("42f00b")->level);
  EXPECT_EQ(H264Level::kLevel4_2, ParseH264ProfileLevelId("42C02A")->level);
  EXPECT_EQ(H264Level::kLevel5_2, ParseH264ProfileLevelId("640c34")->level);
}

TEST(H264ProfileLevelId, TestParsingConstrainedBaseline) {
  EXPECT_EQ(H264Profile::kProfileConstrainedBaseline,
            ParseH264ProfileLevelId("42e01f")->profile);
  EXPECT_EQ(H264Profile::kProfileConstrainedBaseline,
            ParseH264ProfileLevelId("42C02A")->profile);
  EXPECT_EQ(H264Profile::kProfileConstrainedBaseline,
            ParseH264ProfileLevelId("4de01f")->profile);
  EXPECT_EQ(H264Profile::kProfileConstrainedBaseline,
            ParseH264ProfileLevelId("58f01f")->profile);
}

TEST(H264ProfileLevelId, TestParsingBaseline) {
  EXPECT_EQ(H264Profile::kProfileBaseline,
            ParseH264ProfileLevelId("42a01f")->profile);
  EXPECT_EQ(H264Profile::kProfileBaseline,
            ParseH264ProfileLevelId("58A01F")->profile);
}

TEST(H264ProfileLevelId, TestParsingMain) {
  EXPECT_EQ(H264Profile::kProfileMain,
            ParseH264ProfileLevelId("4D401f")->profile);
}

TEST(H264ProfileLevelId, TestParsingHigh) {
  EXPECT_EQ(H264Profile::kProfileHigh,
            ParseH264ProfileLevelId("64001f")->profile);
}

TEST(H264ProfileLevelId, TestParsingConstrainedHigh) {
  EXPECT_EQ(H264Profile::kProfileConstrainedHigh,
            ParseH264ProfileLevelId("640c1f")->profile);
}

TEST(H264ProfileLevelId, TestSupportedLevel) {
  EXPECT_EQ(H264Level::kLevel2_1, *H264SupportedLevel(640 * 480, 25));
  EXPECT_EQ(H264Level::kLevel3_1, *H264SupportedLevel(1280 * 720, 30));
  EXPECT_EQ(H264Level::kLevel4_2, *H264SupportedLevel(1920 * 1280, 60));
}

// Test supported level below level 1 requirements.
TEST(H264ProfileLevelId, TestSupportedLevelInvalid) {
  EXPECT_FALSE(H264SupportedLevel(0, 0));
  // All levels support fps > 5.
  EXPECT_FALSE(H264SupportedLevel(1280 * 720, 5));
  // All levels support frame sizes > 183 * 137.
  EXPECT_FALSE(H264SupportedLevel(183 * 137, 30));
}

TEST(H264ProfileLevelId, TestToString) {
  EXPECT_EQ("42e01f", *H264ProfileLevelIdToString(H264ProfileLevelId(
                          H264Profile::kProfileConstrainedBaseline,
                          H264Level::kLevel3_1)));
  EXPECT_EQ("42000a", *H264ProfileLevelIdToString(H264ProfileLevelId(
                          H264Profile::kProfileBaseline, H264Level::kLevel1)));
  EXPECT_EQ("4d001f", H264ProfileLevelIdToString(H264ProfileLevelId(
                          H264Profile::kProfileMain, H264Level::kLevel3_1)));
  EXPECT_EQ("640c2a",
            *H264ProfileLevelIdToString(H264ProfileLevelId(
                H264Profile::kProfileConstrainedHigh, H264Level::kLevel4_2)));
  EXPECT_EQ("64002a", *H264ProfileLevelIdToString(H264ProfileLevelId(
                          H264Profile::kProfileHigh, H264Level::kLevel4_2)));
}

TEST(H264ProfileLevelId, TestToStringLevel1b) {
  EXPECT_EQ("42f00b", *H264ProfileLevelIdToString(H264ProfileLevelId(
                          H264Profile::kProfileConstrainedBaseline,
                          H264Level::kLevel1_b)));
  EXPECT_EQ("42100b",
            *H264ProfileLevelIdToString(H264ProfileLevelId(
                H264Profile::kProfileBaseline, H264Level::kLevel1_b)));
  EXPECT_EQ("4d100b", *H264ProfileLevelIdToString(H264ProfileLevelId(
                          H264Profile::kProfileMain, H264Level::kLevel1_b)));
}

TEST(H264ProfileLevelId, TestToStringRoundTrip) {
  EXPECT_EQ("42e01f",
            *H264ProfileLevelIdToString(*ParseH264ProfileLevelId("42e01f")));
  EXPECT_EQ("42e01f",
            *H264ProfileLevelIdToString(*ParseH264ProfileLevelId("42E01F")));
  EXPECT_EQ("4d100b",
            *H264ProfileLevelIdToString(*ParseH264ProfileLevelId("4d100b")));
  EXPECT_EQ("4d100b",
            *H264ProfileLevelIdToString(*ParseH264ProfileLevelId("4D100B")));
  EXPECT_EQ("640c2a",
            *H264ProfileLevelIdToString(*ParseH264ProfileLevelId("640c2a")));
  EXPECT_EQ("640c2a",
            *H264ProfileLevelIdToString(*ParseH264ProfileLevelId("640C2A")));
}

TEST(H264ProfileLevelId, TestToStringInvalid) {
  EXPECT_FALSE(H264ProfileLevelIdToString(
      H264ProfileLevelId(H264Profile::kProfileHigh, H264Level::kLevel1_b)));
  EXPECT_FALSE(H264ProfileLevelIdToString(H264ProfileLevelId(
      H264Profile::kProfileConstrainedHigh, H264Level::kLevel1_b)));
  EXPECT_FALSE(H264ProfileLevelIdToString(
      H264ProfileLevelId(static_cast<H264Profile>(255), H264Level::kLevel3_1)));
}

TEST(H264ProfileLevelId, TestParseSdpProfileLevelIdEmpty) {
  const absl::optional<H264ProfileLevelId> profile_level_id =
      ParseSdpForH264ProfileLevelId(SdpVideoFormat::Parameters());
  EXPECT_TRUE(profile_level_id);
  EXPECT_EQ(H264Profile::kProfileConstrainedBaseline,
            profile_level_id->profile);
  EXPECT_EQ(H264Level::kLevel3_1, profile_level_id->level);
}

TEST(H264ProfileLevelId, TestParseSdpProfileLevelIdConstrainedHigh) {
  SdpVideoFormat::Parameters params;
  params["profile-level-id"] = "640c2a";
  const absl::optional<H264ProfileLevelId> profile_level_id =
      ParseSdpForH264ProfileLevelId(params);
  EXPECT_TRUE(profile_level_id);
  EXPECT_EQ(H264Profile::kProfileConstrainedHigh, profile_level_id->profile);
  EXPECT_EQ(H264Level::kLevel4_2, profile_level_id->level);
}

TEST(H264ProfileLevelId, TestParseSdpProfileLevelIdInvalid) {
  SdpVideoFormat::Parameters params;
  params["profile-level-id"] = "foobar";
  EXPECT_FALSE(ParseSdpForH264ProfileLevelId(params));
}

TEST(H264ProfileLevelId, TestGenerateProfileLevelIdForAnswerEmpty) {
  SdpVideoFormat::Parameters answer_params;
  GenerateH264ProfileLevelIdForAnswer(SdpVideoFormat::Parameters(),
                                      SdpVideoFormat::Parameters(),
                                      &answer_params);
  EXPECT_TRUE(answer_params.empty());
}

TEST(H264ProfileLevelId,
     TestGenerateProfileLevelIdForAnswerLevelSymmetryCapped) {
  SdpVideoFormat::Parameters low_level;
  low_level["profile-level-id"] = "42e015";
  SdpVideoFormat::Parameters high_level;
  high_level["profile-level-id"] = "42e01f";

  // Level asymmetry is not allowed; test that answer level is the lower of the
  // local and remote levels.
  SdpVideoFormat::Parameters answer_params;
  GenerateH264ProfileLevelIdForAnswer(low_level /* local_supported */,
                                      high_level /* remote_offered */,
                                      &answer_params);
  EXPECT_EQ("42e015", answer_params["profile-level-id"]);

  SdpVideoFormat::Parameters answer_params2;
  GenerateH264ProfileLevelIdForAnswer(high_level /* local_supported */,
                                      low_level /* remote_offered */,
                                      &answer_params2);
  EXPECT_EQ("42e015", answer_params2["profile-level-id"]);
}

TEST(H264ProfileLevelId,
     TestGenerateProfileLevelIdForAnswerConstrainedBaselineLevelAsymmetry) {
  SdpVideoFormat::Parameters local_params;
  local_params["profile-level-id"] = "42e01f";
  local_params["level-asymmetry-allowed"] = "1";
  SdpVideoFormat::Parameters remote_params;
  remote_params["profile-level-id"] = "42e015";
  remote_params["level-asymmetry-allowed"] = "1";
  SdpVideoFormat::Parameters answer_params;
  GenerateH264ProfileLevelIdForAnswer(local_params, remote_params,
                                      &answer_params);
  // When level asymmetry is allowed, we can answer a higher level than what was
  // offered.
  EXPECT_EQ("42e01f", answer_params["profile-level-id"]);
}

}  // namespace webrtc
