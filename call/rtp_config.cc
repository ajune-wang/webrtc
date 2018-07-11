/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_config.h"

#include "rtc_base/strings/string_builder.h"

namespace webrtc {

std::string NackConfig::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{rtp_history_ms: " << rtp_history_ms;
  ss << '}';
  return ss.str();
}

std::string UlpfecConfig::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{ulpfec_payload_type: " << ulpfec_payload_type;
  ss << ", red_payload_type: " << red_payload_type;
  ss << ", red_rtx_payload_type: " << red_rtx_payload_type;
  ss << '}';
  return ss.str();
}

bool UlpfecConfig::operator==(const UlpfecConfig& other) const {
  return ulpfec_payload_type == other.ulpfec_payload_type &&
         red_payload_type == other.red_payload_type &&
         red_rtx_payload_type == other.red_rtx_payload_type;
}

Rtp::Rtp() = default;
Rtp::Rtp(const Rtp&) = default;
Rtp::~Rtp() = default;

Rtp::Flexfec::Flexfec() = default;
Rtp::Flexfec::Flexfec(const Flexfec&) = default;
Rtp::Flexfec::~Flexfec() = default;

std::string Rtp::ToString() const {
  char buf[2 * 1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", rtcp_mode: "
     << (rtcp_mode == RtcpMode::kCompound ? "RtcpMode::kCompound"
                                          : "RtcpMode::kReducedSize");
  ss << ", max_packet_size: " << max_packet_size;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", ulpfec: " << ulpfec.ToString();
  ss << ", payload_name: " << payload_name;
  ss << ", payload_type: " << payload_type;

  ss << ", flexfec: {payload_type: " << flexfec.payload_type;
  ss << ", ssrc: " << flexfec.ssrc;
  ss << ", protected_media_ssrcs: [";
  for (size_t i = 0; i < flexfec.protected_media_ssrcs.size(); ++i) {
    ss << flexfec.protected_media_ssrcs[i];
    if (i != flexfec.protected_media_ssrcs.size() - 1)
      ss << ", ";
  }
  ss << "]}";

  ss << ", rtx: " << rtx.ToString();
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

Rtp::Rtx::Rtx() = default;
Rtp::Rtx::Rtx(const Rtx&) = default;
Rtp::Rtx::~Rtx() = default;

std::string Rtp::Rtx::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", payload_type: " << payload_type;
  ss << '}';
  return ss.str();
}

Rtcp::Rtcp() = default;
Rtcp::Rtcp(const Rtcp&) = default;
Rtcp::~Rtcp() = default;

std::string Rtcp::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{video_report_interval_ms: " << video_report_interval_ms;
  ss << ", audio_report_interval_ms: " << audio_report_interval_ms;
  ss << '}';
  return ss.str();
}

}  // namespace webrtc
