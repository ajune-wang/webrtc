/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/struct_parameters_parser.h"

#include <algorithm>

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace struct_parser_impl {
namespace {
size_t FindOrEnd(absl::string_view str, size_t start, char delimiter) {
  size_t pos = str.find(delimiter, start);
  pos = (pos == std::string::npos) ? str.length() : pos;
  return pos;
}
}  // namespace

ParserBase::ParserBase(std::vector<ParameterParser> fields)
    : fields_(std::move(fields)) {
  std::sort(fields_.begin(), fields_.end(),
            [](const ParameterParser& a, const ParameterParser& b) {
              return static_cast<bool>(a.key < b.key);
            });
}

void ParserBase::Parse(void* target, absl::string_view src) const {
  size_t i = 0;
  while (i < src.length()) {
    size_t val_end = FindOrEnd(src, i, ',');
    size_t colon_pos = FindOrEnd(src, i, ':');
    size_t key_end = std::min(val_end, colon_pos);
    size_t val_begin = key_end + 1u;
    absl::string_view key(src.substr(i, key_end - i));
    absl::string_view opt_value;
    if (val_end >= val_begin)
      opt_value = src.substr(val_begin, val_end - val_begin);
    i = val_end + 1u;
    bool found = false;
    for (const auto& parser : fields_) {
      if (key == parser.key) {
        found = true;
        if (!parser.Parse(opt_value, target)) {
          RTC_LOG(LS_WARNING) << "Failed to read field with key: '" << key
                              << "' in trial: \"" << src << "\"";
        }
        break;
      }
    }
    if (!found) {
      RTC_LOG(LS_INFO) << "No field with key: '" << key
                       << "' (found in trial: \"" << src << "\")";
    }
  }
}

std::string ParserBase::EncodeChanged(const void* src, const void* base) const {
  rtc::StringBuilder sb;
  bool first = true;
  for (const auto& parser : fields_) {
    if (parser.Changed(src, base)) {
      if (!first)
        sb << ",";
      sb << parser.key << ":";
      parser.Encode(src, &sb);
      first = false;
    }
  }
  return sb.Release();
}

std::string ParserBase::EncodeAll(const void* src) const {
  rtc::StringBuilder sb;
  bool first = true;
  for (const auto& parser : fields_) {
    if (!first)
      sb << ",";
    sb << parser.key << ":";
    parser.Encode(src, &sb);
    first = false;
  }
  return sb.Release();
}

}  // namespace struct_parser_impl

}  // namespace webrtc
