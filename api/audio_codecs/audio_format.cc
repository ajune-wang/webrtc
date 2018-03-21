/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_format.h"
#include "rtc_base/strings/string_builder.h"

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

SdpAudioFormat::SdpAudioFormat(const SdpAudioFormat&) = default;
SdpAudioFormat::SdpAudioFormat(SdpAudioFormat&&) = default;

SdpAudioFormat::SdpAudioFormat(const char* name,
                               int clockrate_hz,
                               size_t num_channels)
    : name(name), clockrate_hz(clockrate_hz), num_channels(num_channels) {}

SdpAudioFormat::SdpAudioFormat(const std::string& name,
                               int clockrate_hz,
                               size_t num_channels)
    : name(name), clockrate_hz(clockrate_hz), num_channels(num_channels) {}

SdpAudioFormat::SdpAudioFormat(const char* name,
                               int clockrate_hz,
                               size_t num_channels,
                               const Parameters& param)
    : name(name),
      clockrate_hz(clockrate_hz),
      num_channels(num_channels),
      parameters(param) {}

SdpAudioFormat::SdpAudioFormat(const std::string& name,
                               int clockrate_hz,
                               size_t num_channels,
                               const Parameters& param)
    : name(name),
      clockrate_hz(clockrate_hz),
      num_channels(num_channels),
      parameters(param) {}

bool SdpAudioFormat::Matches(const SdpAudioFormat& o) const {
  return STR_CASE_CMP(name.c_str(), o.name.c_str()) == 0 &&
         clockrate_hz == o.clockrate_hz && num_channels == o.num_channels;
}

SdpAudioFormat::~SdpAudioFormat() = default;
SdpAudioFormat& SdpAudioFormat::operator=(const SdpAudioFormat&) = default;
SdpAudioFormat& SdpAudioFormat::operator=(SdpAudioFormat&&) = default;

std::string SdpAudioFormat::ToString() const {
  char sb_buf[1024];
  rtc::SimpleStringBuilder sb(sb_buf);
  sb << "{name: " << name;
  sb << ", clockrate_hz: " << clockrate_hz;
  sb << ", num_channels: " << num_channels;
  sb << ", parameters: {";
  const char* sep = "";
  for (const auto& kv : parameters) {
    sb << sep << kv.first << ": " << kv.second;
    sep = ", ";
  }
  sb << "}}";
  return sb.str();
}

bool operator==(const SdpAudioFormat& a, const SdpAudioFormat& b) {
  return STR_CASE_CMP(a.name.c_str(), b.name.c_str()) == 0 &&
         a.clockrate_hz == b.clockrate_hz && a.num_channels == b.num_channels &&
         a.parameters == b.parameters;
}

void swap(SdpAudioFormat& a, SdpAudioFormat& b) {
  using std::swap;
  swap(a.name, b.name);
  swap(a.clockrate_hz, b.clockrate_hz);
  swap(a.num_channels, b.num_channels);
  swap(a.parameters, b.parameters);
}

std::ostream& operator<<(std::ostream& os, const SdpAudioFormat& saf) {
  os << "{name: " << saf.name;
  os << ", clockrate_hz: " << saf.clockrate_hz;
  os << ", num_channels: " << saf.num_channels;
  os << ", parameters: {";
  const char* sep = "";
  for (const auto& kv : saf.parameters) {
    os << sep << kv.first << ": " << kv.second;
    sep = ", ";
  }
  os << "}}";
  return os;
}

AudioCodecInfo::AudioCodecInfo(int sample_rate_hz,
                               size_t num_channels,
                               int bitrate_bps)
    : AudioCodecInfo(sample_rate_hz,
                     num_channels,
                     bitrate_bps,
                     bitrate_bps,
                     bitrate_bps) {}

AudioCodecInfo::AudioCodecInfo(int sample_rate_hz,
                               size_t num_channels,
                               int default_bitrate_bps,
                               int min_bitrate_bps,
                               int max_bitrate_bps)
    : sample_rate_hz(sample_rate_hz),
      num_channels(num_channels),
      default_bitrate_bps(default_bitrate_bps),
      min_bitrate_bps(min_bitrate_bps),
      max_bitrate_bps(max_bitrate_bps) {
  RTC_DCHECK_GT(sample_rate_hz, 0);
  RTC_DCHECK_GT(num_channels, 0);
  RTC_DCHECK_GE(min_bitrate_bps, 0);
  RTC_DCHECK_LE(min_bitrate_bps, default_bitrate_bps);
  RTC_DCHECK_GE(max_bitrate_bps, default_bitrate_bps);
}

std::string AudioCodecInfo::ToString() const {
  char sb_buf[1024];
  rtc::SimpleStringBuilder sb(sb_buf);
  sb << "{sample_rate_hz: " << sample_rate_hz;
  sb << ", num_channels: " << num_channels;
  sb << ", default_bitrate_bps: " << default_bitrate_bps;
  sb << ", min_bitrate_bps: " << min_bitrate_bps;
  sb << ", max_bitrate_bps: " << max_bitrate_bps;
  sb << ", allow_comfort_noise: " << allow_comfort_noise;
  sb << ", supports_network_adaption: " << supports_network_adaption;
  sb << "}";
  return sb.str();
}

std::ostream& operator<<(std::ostream& os, const AudioCodecInfo& aci) {
  os << "{sample_rate_hz: " << aci.sample_rate_hz;
  os << ", num_channels: " << aci.num_channels;
  os << ", default_bitrate_bps: " << aci.default_bitrate_bps;
  os << ", min_bitrate_bps: " << aci.min_bitrate_bps;
  os << ", max_bitrate_bps: " << aci.max_bitrate_bps;
  os << ", allow_comfort_noise: " << aci.allow_comfort_noise;
  os << ", supports_network_adaption: " << aci.supports_network_adaption;
  os << "}";
  return os;
}

std::string AudioCodecSpec::ToString() const {
  char sb_buf[1024];
  rtc::SimpleStringBuilder sb(sb_buf);
  sb << "{format: " << format.ToString();
  sb << ", info: " << info.ToString();
  sb << "}";
  return sb.str();
}

std::ostream& operator<<(std::ostream& os, const AudioCodecSpec& acs) {
  os << "{format: " << acs.format;
  os << ", info: " << acs.info;
  os << "}";
  return os;
}

}  // namespace webrtc
