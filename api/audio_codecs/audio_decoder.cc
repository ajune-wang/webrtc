/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_decoder.h"

#include <assert.h>

#include <memory>
#include <utility>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/sanitizer.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

bool AudioDecoder::EncodedAudioFrame::IsDtxPacket() const {
  return false;
}

AudioDecoder::ParseResult::ParseResult() = default;
AudioDecoder::ParseResult::ParseResult(ParseResult&& b) = default;
AudioDecoder::ParseResult::ParseResult(uint32_t timestamp,
                                       int priority,
                                       std::unique_ptr<EncodedAudioFrame> frame)
    : timestamp(timestamp), priority(priority), frame(std::move(frame)) {
  RTC_DCHECK_GE(priority, 0);
}

AudioDecoder::ParseResult::~ParseResult() = default;

AudioDecoder::ParseResult& AudioDecoder::ParseResult::operator=(
    ParseResult&& b) = default;

bool AudioDecoder::HasDecodePlc() const {
  return false;
}

size_t AudioDecoder::DecodePlc(size_t num_frames, int16_t* decoded) {
  return 0;
}

// TODO(bugs.webrtc.org/9676): Remove default implementation.
void AudioDecoder::GeneratePlc(size_t /*requested_samples_per_channel*/,
                               rtc::BufferT<int16_t>* /*concealment_audio*/) {}

int AudioDecoder::ErrorCode() {
  return 0;
}

int AudioDecoder::PacketDuration(const uint8_t* encoded,
                                 size_t encoded_len) const {
  return kNotImplemented;
}

int AudioDecoder::PacketDurationRedundant(const uint8_t* encoded,
                                          size_t encoded_len) const {
  return kNotImplemented;
}

bool AudioDecoder::PacketHasFec(const uint8_t* encoded,
                                size_t encoded_len) const {
  return false;
}

AudioDecoder::SpeechType AudioDecoder::ConvertSpeechType(int16_t type) {
  switch (type) {
    case 0:  // TODO(hlundin): Both iSAC and Opus return 0 for speech.
    case 1:
      return kSpeech;
    case 2:
      return kComfortNoise;
    default:
      assert(false);
      return kSpeech;
  }
}

}  // namespace webrtc
