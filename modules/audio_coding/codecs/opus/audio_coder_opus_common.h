/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_CODER_OPUS_COMMON_H_
#define MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_CODER_OPUS_COMMON_H_

#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"  // TODO(aleloi): Replace with smth else...
#include "absl/types/optional.h"
#include "api/audio_codecs/audio_format.h"
#include "rtc_base/string_to_number.h"

namespace webrtc {

absl::optional<std::string> GetFormatParameter(const SdpAudioFormat& format,
                                               const std::string& param) {
  auto it = format.parameters.find(param);
  if (it == format.parameters.end())
    return absl::nullopt;

  return it->second;
}

template <typename T>
absl::optional<T> GetFormatParameter(const SdpAudioFormat& format,
                                     const std::string& param) {
  return rtc::StringToNumber<T>(GetFormatParameter(format, param).value_or(""));
}

template <>
absl::optional<std::vector<int>> GetFormatParameter(
    const SdpAudioFormat& format,
    const std::string& param) {
  std::string comma_separated_list =
      GetFormatParameter(format, param).value_or("");
  std::vector<std::string> splitted_list =
      absl::StrSplit(comma_separated_list, ",");
  std::vector<int> result;

  for (const auto s : splitted_list) {
    auto conv = rtc::StringToNumber<int>(s);
    if (!conv.has_value()) {
      return absl::nullopt;
    }
    result.push_back(*conv);
  }
  return result;
}

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_CODER_OPUS_COMMON_H_
