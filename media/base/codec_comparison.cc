/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/codec_comparison.h"

#include "api/video_codecs/av1_profile.h"
#include "api/video_codecs/h264_profile_level_id.h"
#ifdef RTC_ENABLE_H265
#include "api/video_codecs/h265_profile_tier_level.h"
#endif
#include "absl/strings/match.h"
#include "api/video_codecs/vp9_profile.h"
#include "media/base/media_constants.h"
namespace cricket {

namespace {

std::string GetFmtpParameterOrDefault(const webrtc::CodecParameterMap& params,
                                      const std::string& name,
                                      const std::string& default_value) {
  const auto it = params.find(name);
  if (it != params.end()) {
    return it->second;
  }
  return default_value;
}

std::string H264GetPacketizationModeOrDefault(
    const webrtc::CodecParameterMap& params) {
  // If packetization-mode is not present, default to "0".
  // https://tools.ietf.org/html/rfc6184#section-6.2
  return GetFmtpParameterOrDefault(params, cricket::kH264FmtpPacketizationMode,
                                   "0");
}

bool H264IsSamePacketizationMode(const webrtc::CodecParameterMap& left,
                                 const webrtc::CodecParameterMap& right) {
  return H264GetPacketizationModeOrDefault(left) ==
         H264GetPacketizationModeOrDefault(right);
}

std::string AV1GetTierOrDefault(const webrtc::CodecParameterMap& params) {
  // If the parameter is not present, the tier MUST be inferred to be 0.
  // https://aomediacodec.github.io/av1-rtp-spec/#72-sdp-parameters
  return GetFmtpParameterOrDefault(params, cricket::kAv1FmtpTier, "0");
}

bool AV1IsSameTier(const webrtc::CodecParameterMap& left,
                   const webrtc::CodecParameterMap& right) {
  return AV1GetTierOrDefault(left) == AV1GetTierOrDefault(right);
}

std::string AV1GetLevelIdxOrDefault(const webrtc::CodecParameterMap& params) {
  // If the parameter is not present, it MUST be inferred to be 5 (level 3.1).
  // https://aomediacodec.github.io/av1-rtp-spec/#72-sdp-parameters
  return GetFmtpParameterOrDefault(params, cricket::kAv1FmtpLevelIdx, "5");
}

bool AV1IsSameLevelIdx(const webrtc::CodecParameterMap& left,
                       const webrtc::CodecParameterMap& right) {
  return AV1GetLevelIdxOrDefault(left) == AV1GetLevelIdxOrDefault(right);
}

}  // namespace

// Some (video) codecs are actually families of codecs and rely on parameters
// to distinguish different incompatible family members.
bool IsSameCodecSpecific(const std::string& name1,
                         const webrtc::CodecParameterMap& params1,
                         const std::string& name2,
                         const webrtc::CodecParameterMap& params2) {
  // The names might not necessarily match, so check both.
  auto either_name_matches = [&](const std::string name) {
    return absl::EqualsIgnoreCase(name, name1) ||
           absl::EqualsIgnoreCase(name, name2);
  };
  if (either_name_matches(kH264CodecName))
    webrtc::H264IsSameProfile(params1, params2) &&
        H264IsSamePacketizationMode(params1, params2);
  if (either_name_matches(kVp9CodecName))
    return webrtc::VP9IsSameProfile(params1, params2);
  if (either_name_matches(kAv1CodecName))
    return webrtc::AV1IsSameProfile(params1, params2) &&
           AV1IsSameTier(params1, params2) &&
           AV1IsSameLevelIdx(params1, params2);
#ifdef RTC_ENABLE_H265
  if (either_name_matches(kH265CodecName)) {
    webrtc::H265IsSameProfileTierLevel(params1, params2) &&
        webrtc::IsSameH265TxMode(params1, params2);
  }
#endif
  return true;
}

}  // namespace cricket
