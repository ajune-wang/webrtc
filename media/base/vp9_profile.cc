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

// Profile information for VP9 video.
const char kVP9Profile[] = "profile";

std::string VP9ProfileToString(VP9Profile profile) {
  switch (profile) {
    case VP9Profile::kProfile0:
      return "0";
    case VP9Profile::kProfile2:
      return "2";
  }
  return "0";
}

VP9Profile StringToVp9Profile(const std::string& str) {
  const int i = std::stoi(str);
  switch (i) {
    case 0:
      return VP9Profile::kProfile0;
    case 2:
      return VP9Profile::kProfile2;
    default:
      RTC_NOTREACHED();
  }
  return VP9Profile::kProfile0;
}

rtc::Optional<VP9Profile> ParseSdpForVP9Profile(
    const SdpVideoFormat::Parameters& params) {
  const auto profile_it = params.find(kVP9Profile);
  if (profile_it == params.end())
    return VP9Profile::kProfile0;
  const std::string& profile_str = profile_it->second;
  return StringToVp9Profile(profile_str);
}

bool IsSameVP9Profile(const SdpVideoFormat::Parameters& params1,
                      const SdpVideoFormat::Parameters& params2) {
  const rtc::Optional<VP9Profile> profile = ParseSdpForVP9Profile(params1);
  const rtc::Optional<VP9Profile> other_profile =
      ParseSdpForVP9Profile(params2);
  return profile == other_profile;
}

}  // namespace webrtc
