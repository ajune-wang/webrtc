/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_input.h"

#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace test {

NetEqInput::PacketData::PacketData() = default;
NetEqInput::PacketData::~PacketData() = default;

std::string NetEqInput::PacketData::ToString() const {
  rtc::StringBuilder ss;
  ss << "{"
        "time_ms: "
     << static_cast<int64_t>(time_ms)
     << ", "
        "header: {"
        "pt: "
     << static_cast<int>(header.payloadType)
     << ", "
        "sn: "
     << header.sequenceNumber
     << ", "
        "ts: "
     << header.timestamp
     << ", "
        "ssrc: "
     << header.ssrc
     << "}, "
        "payload bytes: "
     << payload.size() << "}";
  return ss.Release();
}

TimeLimitedNetEqInput::TimeLimitedNetEqInput(std::unique_ptr<NetEqInput> input,
                                             int64_t duration_ms)
    : input_(std::move(input)),
      start_time_ms_(input_->NextEventTime()),
      duration_ms_(duration_ms) {}

TimeLimitedNetEqInput::~TimeLimitedNetEqInput() = default;

NetEqInput::Event TimeLimitedNetEqInput::PopEvent() {
  Event event;
  if (ended_) {
    return event;
  }
  event = input_->PopEvent();
  MaybeSetEnded();
  return event;
}

bool TimeLimitedNetEqInput::ended() const {
  return ended_ || input_->ended();
}

const NetEqInput::Event& TimeLimitedNetEqInput::NextEvent() const {
  return ended_ ? empty_event_ : input_->NextEvent();
}

absl::optional<RTPHeader> TimeLimitedNetEqInput::NextHeader() const {
  return ended_ ? absl::nullopt : input_->NextHeader();
}

void TimeLimitedNetEqInput::MaybeSetEnded() {
  if (NextEventTime() && start_time_ms_ &&
      *NextEventTime() - *start_time_ms_ > duration_ms_) {
    ended_ = true;
  }
}

}  // namespace test
}  // namespace webrtc
