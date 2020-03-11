//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#include "audio/voip/audio_egress.h"

#include <utility>
#include <vector>

#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr int kTelephoneEventAttenuationdB = 10;

}  // namespace

AudioEgress::AudioEgress(RtpRtcp* rtp_rtcp, Clock* clock)
    : rtp_rtcp_(rtp_rtcp),
      rtp_sender_audio_(
          std::make_unique<RTPSenderAudio>(clock, rtp_rtcp_->RtpSender())),
      audio_coding_(AudioCodingModule::Create(AudioCodingModule::Config())) {
  audio_thread_checker_.Detach();
  int status = audio_coding_->RegisterTransportCallback(this);
  RTC_DCHECK_EQ(status, 0);
}

AudioEgress::~AudioEgress() {
  if (IsSending()) {
    StopSend();
  }
  int status = audio_coding_->RegisterTransportCallback(nullptr);
  RTC_DCHECK_EQ(status, 0);
}

bool AudioEgress::IsSending() const {
  RTC_DCHECK_RUN_ON(&app_thread_checker_);
  return rtp_rtcp_->SendingMedia();
}

void AudioEgress::SetEncoder(int payload_type,
                             const SdpAudioFormat& encoder_format,
                             std::unique_ptr<AudioEncoder> encoder) {
  RTC_DCHECK_RUN_ON(&app_thread_checker_);
  RTC_DCHECK_GE(payload_type, 0);
  RTC_DCHECK_LE(payload_type, 127);

  encoder_format_ = encoder_format;

  // The RTP/RTCP module needs to know the RTP timestamp rate (i.e. clockrate)
  // as well as some other things, so we collect this info and send it along.
  rtp_rtcp_->RegisterSendPayloadFrequency(payload_type,
                                          encoder->RtpTimestampRateHz());
  rtp_sender_audio_->RegisterAudioPayload("audio", payload_type,
                                          encoder->RtpTimestampRateHz(),
                                          encoder->NumChannels(), 0);

  audio_coding_->SetEncoder(std::move(encoder));
}

absl::optional<SdpAudioFormat> AudioEgress::GetEncoderFormat() {
  RTC_DCHECK_RUN_ON(&app_thread_checker_);
  return encoder_format_;
}

void AudioEgress::StartSend() {
  RTC_DCHECK_RUN_ON(&app_thread_checker_);
  RTC_DCHECK(!IsSending());

  rtp_rtcp_->SetSendingMediaStatus(true);
}

void AudioEgress::StopSend() {
  RTC_DCHECK_RUN_ON(&app_thread_checker_);
  RTC_DCHECK(IsSending());

  rtp_rtcp_->SetSendingMediaStatus(false);
}

void AudioEgress::SendAudioData(std::unique_ptr<AudioFrame> audio_frame) {
  RTC_DCHECK_RUN_ON(&audio_thread_checker_);

  RTC_DCHECK_GT(audio_frame->samples_per_channel_, 0);
  RTC_DCHECK_LE(audio_frame->num_channels_, 8);

  bool is_muted = mute_;
  AudioFrameOperations::Mute(audio_frame.get(), previously_muted_, is_muted);
  previously_muted_ = is_muted;

  // The ACM resamples internally.
  audio_frame->timestamp_ = next_frame_rtp_timestamp_;

  // This call will trigger AudioPacketizationCallback::SendData if
  // encoding is done and payload is ready for packetization and
  // transmission. Otherwise, it will return without invoking the
  // callback.
  if (audio_coding_->Add10MsData(*audio_frame) < 0) {
    RTC_DLOG(LS_ERROR) << "ACM::Add10MsData() failed.";
    return;
  }

  next_frame_rtp_timestamp_ +=
      rtc::dchecked_cast<uint32_t>(audio_frame->samples_per_channel_);
}

int32_t AudioEgress::SendData(AudioFrameType frame_type,
                              uint8_t payload_type,
                              uint32_t timestamp,
                              const uint8_t* payload_data,
                              size_t payload_size) {
  RTC_DCHECK_RUN_ON(&audio_thread_checker_);

  rtc::ArrayView<const uint8_t> payload(payload_data, payload_size);

  // Leaving the time when this frame was received from the capture device as
  // undefined for voice for now.
  constexpr uint32_t kUndefinedCaptureTime = -1;

  // Push data from ACM to RTP/RTCP-module to deliver audio frame for
  // packetization.
  if (!rtp_rtcp_->OnSendingRtpFrame(timestamp, kUndefinedCaptureTime,
                                    payload_type,
                                    /*force_sender_report=*/false)) {
    return -1;
  }

  const uint32_t rtp_timestamp = timestamp + rtp_rtcp_->StartTimestamp();

  // This call will trigger Transport::SendPacket() from the RTP/RTCP module.
  if (!rtp_sender_audio_->SendAudio(frame_type, payload_type, rtp_timestamp,
                                    payload.data(), payload.size())) {
    RTC_DLOG(LS_ERROR)
        << "AudioEgress::SendData() failed to send data to RTP/RTCP module";
    return -1;
  }

  return 0;
}

void AudioEgress::RegisterTelephoneEventType(int rtp_payload_type,
                                             int sample_rate_hz) {
  RTC_DCHECK_RUN_ON(&app_thread_checker_);
  RTC_DCHECK_GE(rtp_payload_type, 0);
  RTC_DCHECK_LE(rtp_payload_type, 127);

  rtp_rtcp_->RegisterSendPayloadFrequency(rtp_payload_type, sample_rate_hz);
  rtp_sender_audio_->RegisterAudioPayload("telephone-event", rtp_payload_type,
                                          sample_rate_hz, 0, 0);
}

bool AudioEgress::SendTelephoneEvent(int event, int duration_ms) {
  RTC_DCHECK_RUN_ON(&app_thread_checker_);
  RTC_DCHECK_GE(event, 0);
  RTC_DCHECK_LE(event, 255);
  RTC_DCHECK_GE(duration_ms, 0);
  RTC_DCHECK_LE(duration_ms, 65535);
  RTC_DCHECK(IsSending());

  if (rtp_sender_audio_->SendTelephoneEvent(
          event, duration_ms, kTelephoneEventAttenuationdB) != 0) {
    RTC_DLOG(LS_ERROR) << "SendTelephoneEvent() failed to send event";
    return false;
  }
  return true;
}

void AudioEgress::SetMute(bool mute) {
  RTC_DCHECK_RUN_ON(&app_thread_checker_);
  mute_ = mute;
}

}  // namespace webrtc
