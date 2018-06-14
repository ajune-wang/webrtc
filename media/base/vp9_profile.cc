/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/vp9_profile.h"

namespace webrtc {
namespace VP9 {

// Profile information for VP9 video.
const char kVP9Profile[] = "profile";

rtc::Optional<Profile> ParseSdpProfile(
    const SdpVideoFormat::Parameters& params) {
  const auto profile_it = params.find(kVP9Profile);
  if (profile_it == params.end())
    return kProfile0;
  const std::string& profile_str = profile_it->second;
  return static_cast<Profile>(std::stoi(profile_str));
}

bool IsSameVP9Profile(const SdpVideoFormat::Parameters& params1,
                      const SdpVideoFormat::Parameters& params2) {
  const rtc::Optional<Profile> profile = ParseSdpProfile(params1);
  const rtc::Optional<Profile> other_profile = ParseSdpProfile(params2);
  return profile == other_profile;
}

}  // namespace VP9
}  // namespace webrtc
