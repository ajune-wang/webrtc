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

#include "rtc_base/event.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr int kTelephoneEventAttenuationdB = 10;

}  // namespace

AudioEgress::AudioEgress(RtpRtcp* rtp_rtcp,
                         Clock* clock,
                         TaskQueueFactory* task_queue_factory)
    : rtp_rtcp_(rtp_rtcp),
      rtp_sender_audio_(
          std::make_unique<RTPSenderAudio>(clock, rtp_rtcp_->RtpSender())),
      audio_coding_(AudioCodingModule::Create(AudioCodingModule::Config())),
      encoder_queue_(task_queue_factory->CreateTaskQueue(
          "AudioEncoder",
          TaskQueueFactory::Priority::NORMAL)) {
  int status = audio_coding_->RegisterTransportCallback(this);
  RTC_DCHECK_EQ(0, status);
}

AudioEgress::~AudioEgress() {
  if (IsSending()) {
    StopSend();
  }
  int status = audio_coding_->RegisterTransportCallback(nullptr);
  RTC_DCHECK_EQ(0, status);
}

bool AudioEgress::IsSending() const {
  return rtp_rtcp_->SendingMedia();
}

void AudioEgress::SetEncoder(int payload_type,
                             const SdpAudioFormat& encoder_format,
                             std::unique_ptr<AudioEncoder> encoder) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
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

int AudioEgress::EncoderSampleRate() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (encoder_format_) {
    return encoder_format_->clockrate_hz;
  }
  return 0;
}

size_t AudioEgress::EncoderNumChannel() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (encoder_format_) {
    return encoder_format_->num_channels;
  }
  return 0;
}

int32_t AudioEgress::SendData(AudioFrameType frame_type,
                              uint8_t payload_type,
                              uint32_t timestamp,
                              const uint8_t* payload_data,
                              size_t payload_size) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

  rtc::ArrayView<const uint8_t> payload(payload_data, payload_size);

  // Push data from ACM to RTP/RTCP-module to deliver audio frame for
  // packetization.
  if (!rtp_rtcp_->OnSendingRtpFrame(timestamp,
                                    // Leaving the time when this frame was
                                    // received from the capture device as
                                    // undefined for voice for now.
                                    /*capture_time_ms=*/-1, payload_type,
                                    /*force_sender_report=*/false)) {
    return -1;
  }

  // This block of code follows the logic from SendData from channel_send.cc,
  // and it should reflect the same idea here.
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

void AudioEgress::StartSend() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(!IsSending());

  rtp_rtcp_->SetSendingMediaStatus(true);

  // It is now OK to start processing on the encoder task queue.
  encoder_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    active_encoder_queue_ = true;
  });
}

void AudioEgress::StopSend() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(INFO) << "StopSend::IsSending(): " << IsSending();
  RTC_DCHECK(IsSending());

  rtc::Event flush;
  encoder_queue_.PostTask([this, &flush]() {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    active_encoder_queue_ = false;
    flush.Set();
  });
  flush.Wait(rtc::Event::kForever);

  rtp_rtcp_->SetSendingMediaStatus(false);
}

void AudioEgress::SendAudioData(std::unique_ptr<AudioFrame> audio_frame) {
  RTC_DCHECK_GT(audio_frame->samples_per_channel_, 0);
  RTC_DCHECK_LE(audio_frame->num_channels_, 8);

  // Profile time between when the audio frame is added to the task queue and
  // when the task is actually executed.
  audio_frame->UpdateProfileTimeStamp();

  encoder_queue_.PostTask(
      [this, audio_frame = std::move(audio_frame)]() mutable {
        RTC_DCHECK_RUN_ON(&encoder_queue_);
        if (!active_encoder_queue_) {
          return;
        }

        ProcessMuteState(audio_frame.get());

        // The ACM resamples internally.
        audio_frame->timestamp_ = rtp_timestamp_offset_;
        // This call will trigger AudioPacketizationCallback::SendData if
        // encoding is done and payload is ready for packetization and
        // transmission. Otherwise, it will return without invoking the
        // callback.
        if (audio_coding_->Add10MsData(*audio_frame) < 0) {
          RTC_DLOG(LS_ERROR) << "ACM::Add10MsData() failed.";
          return;
        }
        rtp_timestamp_offset_ +=
            rtc::dchecked_cast<uint32_t>(audio_frame->samples_per_channel_);
      });
}

void AudioEgress::ProcessMuteState(AudioFrame* audio_frame) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  bool is_muted = mute_;
  AudioFrameOperations::Mute(audio_frame, previously_muted_, is_muted);
  previously_muted_ = is_muted;
}

void AudioEgress::RegisterTelephoneEventType(int rtp_payload_type,
                                             int sampling_rate_hz) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_LE(0, rtp_payload_type);
  RTC_DCHECK_GE(127, rtp_payload_type);
  rtp_rtcp_->RegisterSendPayloadFrequency(rtp_payload_type, sampling_rate_hz);
  rtp_sender_audio_->RegisterAudioPayload("telephone-event", rtp_payload_type,
                                          sampling_rate_hz, 0, 0);
}

bool AudioEgress::SendTelephoneEvent(int event, int duration_ms) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_LE(0, event);
  RTC_DCHECK_GE(255, event);
  RTC_DCHECK_LE(0, duration_ms);
  RTC_DCHECK_GE(65535, duration_ms);
  RTC_DCHECK(IsSending());

  if (rtp_sender_audio_->SendTelephoneEvent(
          event, duration_ms, kTelephoneEventAttenuationdB) != 0) {
    RTC_DLOG(LS_ERROR) << "SendTelephoneEvent() failed to send event";
    return false;
  }
  return true;
}

void AudioEgress::SetMute(bool mute) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  mute_ = mute;
}

}  // namespace webrtc
