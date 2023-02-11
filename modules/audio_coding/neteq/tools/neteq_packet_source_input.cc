/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_packet_source_input.h"

#include <algorithm>
#include <limits>

#include "absl/strings/string_view.h"
#include "modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

NetEqPacketSourceInput::NetEqPacketSourceInput(
    std::unique_ptr<PacketSource> packet_source)
    : packet_source_(std::move(packet_source)), next_output_event_ms_(0) {
  LoadNextPacket();
  event_ = GetNextEvent();
}

void NetEqPacketSourceInput::LoadNextPacket() {
  packet_ = packet_source_->NextPacket();
}

NetEqInput::Event NetEqPacketSourceInput::PopEvent() {
  Event event_to_return = std::move(event_);
  event_ = GetNextEvent();
  return event_to_return;
}

absl::optional<RTPHeader> NetEqPacketSourceInput::NextHeader() const {
  if (packet_) {
    return packet_->header();
  }
  return absl::nullopt;
}

NetEqInput::Event NetEqPacketSourceInput::GetNextEvent() {
  NetEqInput::Event event;
  if (!packet_) {
    return event;
  }
  if (packet_->time_ms() > next_output_event_ms_) {
    event.audio_output =
        std::make_unique<NetEqInput::GetAudio>(next_output_event_ms_);
    next_output_event_ms_ += kOutputPeriodMs;
    return event;
  }
  std::unique_ptr<PacketData> packet_data(new PacketData);
  packet_data->header = packet_->header();
  if (packet_->payload_length_bytes() == 0 &&
      packet_->virtual_payload_length_bytes() > 0) {
    // This is a header-only "dummy" packet. Set the payload to all zeros, with
    // length according to the virtual length.
    packet_data->payload.SetSize(packet_->virtual_payload_length_bytes());
    std::fill_n(packet_data->payload.data(), packet_data->payload.size(), 0);
  } else {
    packet_data->payload.SetData(packet_->payload(),
                                 packet_->payload_length_bytes());
  }
  packet_data->time_ms = packet_->time_ms();
  event.packet_data = std::move(packet_data);
  LoadNextPacket();
  return event;
}

std::unique_ptr<PacketSource> CreatePacketSouceFromRtpDumpInput(
    absl::string_view file_name,
    const NetEqPacketSourceInput::RtpHeaderExtensionMap& hdr_ext_map,
    absl::optional<uint32_t> ssrc_filter) {
  std::unique_ptr<RtpFileSource> source(
      RtpFileSource::Create(file_name, ssrc_filter));
  for (const auto& ext_pair : hdr_ext_map) {
    source->RegisterRtpHeaderExtension(ext_pair.second, ext_pair.first);
  }
  return source;
}

}  // namespace test
}  // namespace webrtc
