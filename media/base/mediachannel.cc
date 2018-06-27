/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/mediachannel.h"

#include "rtc_base/absl_str_cat.h"

namespace cricket {

VideoOptions::VideoOptions() = default;
VideoOptions::~VideoOptions() = default;

std::string VideoOptions::ToString() const {
  return absl::StrCat(
      "VideoOptions {", ToStringIfSet("noise reduction", video_noise_reduction),
      ToStringIfSet("screencast min bitrate kbps", screencast_min_bitrate_kbps),
      ToStringIfSet("is_screencast ", is_screencast), "}");
}

std::string RtpHeaderExtension::ToString() const {
  return absl::StrCat("{uri: ", uri, ", id: ", id, "}");
}

void MediaChannel::SetInterface(NetworkInterface* iface) {
  rtc::CritScope cs(&network_interface_crit_);
  network_interface_ = iface;
  SetDscp(enable_dscp_ ? PreferredDscp() : rtc::DSCP_DEFAULT);
}

rtc::DiffServCodePoint MediaChannel::PreferredDscp() const {
  return rtc::DSCP_DEFAULT;
}

int MediaChannel::GetRtpSendTimeExtnId() const {
  return -1;
}

MediaSenderInfo::MediaSenderInfo() = default;
MediaSenderInfo::~MediaSenderInfo() = default;

MediaReceiverInfo::MediaReceiverInfo() = default;
MediaReceiverInfo::~MediaReceiverInfo() = default;

VoiceSenderInfo::VoiceSenderInfo() = default;
VoiceSenderInfo::~VoiceSenderInfo() = default;

VoiceReceiverInfo::VoiceReceiverInfo() = default;
VoiceReceiverInfo::~VoiceReceiverInfo() = default;

VideoSenderInfo::VideoSenderInfo() = default;
VideoSenderInfo::~VideoSenderInfo() = default;

VideoReceiverInfo::VideoReceiverInfo() = default;
VideoReceiverInfo::~VideoReceiverInfo() = default;

VoiceMediaInfo::VoiceMediaInfo() = default;
VoiceMediaInfo::~VoiceMediaInfo() = default;

VideoMediaInfo::VideoMediaInfo() = default;
VideoMediaInfo::~VideoMediaInfo() = default;

DataMediaInfo::DataMediaInfo() = default;
DataMediaInfo::~DataMediaInfo() = default;

AudioSendParameters::AudioSendParameters() = default;
AudioSendParameters::~AudioSendParameters() = default;

std::map<std::string, std::string> AudioSendParameters::ToStringMap() const {
  auto params = RtpSendParameters<AudioCodec>::ToStringMap();
  params["options"] = options.ToString();
  return params;
}

VideoSendParameters::VideoSendParameters() = default;
VideoSendParameters::~VideoSendParameters() = default;

std::map<std::string, std::string> VideoSendParameters::ToStringMap() const {
  auto params = RtpSendParameters<VideoCodec>::ToStringMap();
  params["conference_mode"] = (conference_mode ? "yes" : "no");
  return params;
}

DataMediaChannel::DataMediaChannel() = default;
DataMediaChannel::DataMediaChannel(const MediaConfig& config)
    : MediaChannel(config) {}
DataMediaChannel::~DataMediaChannel() = default;

bool DataMediaChannel::GetStats(DataMediaInfo* info) {
  return true;
}

template <class T>
std::string VectorToString(const std::vector<T>& vals) {
  if (vals.size() == 0)
    return "[]";
  std::string s = absl::StrCat("[", vals[0].ToString());
  for (size_t i = 1; i < vals.size(); ++i) {
    absl::StrAppend(&s, ", ", vals[i].ToString());
  }
  absl::StrAppend(&s, "]");
  return s;
}

template std::string VectorToString<AudioCodec>(
    const std::vector<AudioCodec>& vals);
template std::string VectorToString<DataCodec>(
    const std::vector<DataCodec>& vals);
template std::string VectorToString<VideoCodec>(
    const std::vector<VideoCodec>& vals);
template std::string VectorToString<webrtc::RtpExtension>(
    const std::vector<webrtc::RtpExtension>& vals);

}  // namespace cricket
