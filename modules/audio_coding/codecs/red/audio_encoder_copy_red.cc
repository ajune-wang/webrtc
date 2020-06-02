/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/red/audio_encoder_copy_red.h"

#include <string.h>

#include <utility>
#include <vector>

#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"

namespace webrtc {

AudioEncoderCopyRed::Config::Config() = default;
AudioEncoderCopyRed::Config::Config(Config&&) = default;
AudioEncoderCopyRed::Config::~Config() = default;

AudioEncoderCopyRed::AudioEncoderCopyRed(Config&& config)
    : speech_encoder_(std::move(config.speech_encoder)),
      red_payload_type_(config.payload_type) {
  RTC_CHECK(speech_encoder_) << "Speech encoder not provided.";
}

AudioEncoderCopyRed::~AudioEncoderCopyRed() = default;

int AudioEncoderCopyRed::SampleRateHz() const {
  return speech_encoder_->SampleRateHz();
}

size_t AudioEncoderCopyRed::NumChannels() const {
  return speech_encoder_->NumChannels();
}

int AudioEncoderCopyRed::RtpTimestampRateHz() const {
  return speech_encoder_->RtpTimestampRateHz();
}

size_t AudioEncoderCopyRed::Num10MsFramesInNextPacket() const {
  return speech_encoder_->Num10MsFramesInNextPacket();
}

size_t AudioEncoderCopyRed::Max10MsFramesInAPacket() const {
  return speech_encoder_->Max10MsFramesInAPacket();
}

int AudioEncoderCopyRed::GetTargetBitrate() const {
  return speech_encoder_->GetTargetBitrate();
}

AudioEncoder::EncodedInfo AudioEncoderCopyRed::EncodeImpl(
    uint32_t rtp_timestamp,
    rtc::ArrayView<const int16_t> audio,
    rtc::Buffer* encoded) {
  // Allocate room for RFC 2198 header.
  rtc::Buffer header(secondary_info_.encoded_bytes > 0 ? 5 : 0);
  encoded->AppendData(header);

  const size_t primary_offset = encoded->size();
  size_t secondary_offset = 0;

  if (secondary_info_.encoded_bytes > 0) {
    encoded->AppendData(secondary_encoded_);
    secondary_offset = secondary_info_.encoded_bytes;
  }
  EncodedInfo info = speech_encoder_->Encode(rtp_timestamp, audio, encoded);

  if (info.encoded_bytes == 0) {
    encoded->Clear();
    return info;
  }

  // Actually construct the RFC 2198 header.
  if (secondary_info_.encoded_bytes > 0) {
    // TODO(fippo): oddly rtp_timestamp - secondary_info_.encoded_timestamp is
    // 1440, not 960. Therefore we compare the encoded timestamps.
    const uint32_t timestamp_delta =
        info.encoded_timestamp > secondary_info_.encoded_timestamp
            ? info.encoded_timestamp - secondary_info_.encoded_timestamp
            : secondary_info_.encoded_timestamp - info.encoded_timestamp;

    encoded->data()[0] = secondary_info_.payload_type | 0x80;
    rtc::SetBE16(static_cast<uint8_t*>(encoded->data()) + 1,
                 (timestamp_delta << 2) | (secondary_info_.encoded_bytes >> 8));
    encoded->data()[3] = secondary_info_.encoded_bytes & 0xff;
  }
  // TODO(fippo): with DTX we get a TS delta of up to 19200 (i.e. 400ms).
  // We might not want to send very old RED in that case.

  if (secondary_info_.encoded_bytes > 0) {
    encoded->data()[4] = info.payload_type;
  } else {
    // encoded->data()[0] = info.payload_type;
  }

  RTC_CHECK(info.redundant.empty()) << "Cannot use nested redundant encoders.";
  RTC_DCHECK_EQ(encoded->size() - primary_offset - secondary_offset,
                info.encoded_bytes);

  if (info.encoded_bytes > 0) {
    // |info| will be implicitly cast to an EncodedInfoLeaf struct, effectively
    // discarding the (empty) vector of redundant information. This is
    // intentional.
    info.redundant.push_back(info);
    RTC_DCHECK_EQ(info.redundant.size(), 1);
    if (secondary_info_.encoded_bytes > 0) {
      info.redundant.push_back(secondary_info_);
      RTC_DCHECK_EQ(info.redundant.size(), 2);
    }
    // Save primary to secondary.
    secondary_encoded_.SetData(
        encoded->data() + primary_offset + secondary_info_.encoded_bytes,
        info.encoded_bytes);
    secondary_info_ = info;
    RTC_DCHECK_EQ(info.speech, info.redundant[0].speech);
  }

  // Update main EncodedInfo.
  if (header.size() > 0) {
    info.payload_type = red_payload_type_;
  }
  info.encoded_bytes = primary_offset;
  for (std::vector<EncodedInfoLeaf>::const_iterator it = info.redundant.begin();
       it != info.redundant.end(); ++it) {
    info.encoded_bytes += it->encoded_bytes;
  }
  return info;
}

void AudioEncoderCopyRed::Reset() {
  speech_encoder_->Reset();
  secondary_encoded_.Clear();
  secondary_info_.encoded_bytes = 0;
}

bool AudioEncoderCopyRed::SetFec(bool enable) {
  return speech_encoder_->SetFec(enable);
}

bool AudioEncoderCopyRed::SetDtx(bool enable) {
  return speech_encoder_->SetDtx(enable);
}

bool AudioEncoderCopyRed::SetApplication(Application application) {
  return speech_encoder_->SetApplication(application);
}

void AudioEncoderCopyRed::SetMaxPlaybackRate(int frequency_hz) {
  speech_encoder_->SetMaxPlaybackRate(frequency_hz);
}

rtc::ArrayView<std::unique_ptr<AudioEncoder>>
AudioEncoderCopyRed::ReclaimContainedEncoders() {
  return rtc::ArrayView<std::unique_ptr<AudioEncoder>>(&speech_encoder_, 1);
}

void AudioEncoderCopyRed::OnReceivedUplinkPacketLossFraction(
    float uplink_packet_loss_fraction) {
  speech_encoder_->OnReceivedUplinkPacketLossFraction(
      uplink_packet_loss_fraction);
}

void AudioEncoderCopyRed::OnReceivedUplinkBandwidth(
    int target_audio_bitrate_bps,
    absl::optional<int64_t> bwe_period_ms) {
  speech_encoder_->OnReceivedUplinkBandwidth(target_audio_bitrate_bps,
                                             bwe_period_ms);
}

absl::optional<std::pair<TimeDelta, TimeDelta>>
AudioEncoderCopyRed::GetFrameLengthRange() const {
  return speech_encoder_->GetFrameLengthRange();
}

}  // namespace webrtc
