/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/voip/voip_core.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "api/audio_codecs/audio_format.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/logging.h"

namespace webrtc {

// On Windows Vista and newer, Microsoft introduced the concept of "Default
// Communications Device". This means that there are two types of default
// devices (old Wave Audio style default and Default Communications Device).
//
// On Windows systems which only support Wave Audio style default, uses either
// -1 or 0 to select the default device.
//
// Using a #define for AUDIO_DEVICE_ID since we will call *different* versions
// of the ADM functions, depending on the ID type.
#if defined(WEBRTC_WIN)
#define AUDIO_DEVICE_ID \
  (AudioDeviceModule::WindowsDeviceType::kDefaultCommunicationDevice)
#else
#define AUDIO_DEVICE_ID (0u)
#endif  // defined(WEBRTC_WIN)

VoipBase& VoipCore::Base() {
  return *this;
}

VoipNetwork& VoipCore::Network() {
  return *this;
}

VoipCodec& VoipCore::Codec() {
  return *this;
}

bool VoipCore::Init(std::unique_ptr<TaskQueueFactory> task_queue_factory,
                    std::unique_ptr<AudioProcessing> audio_processing,
                    rtc::scoped_refptr<AudioDeviceModule> audio_device,
                    rtc::scoped_refptr<AudioEncoderFactory> encoder_factory,
                    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory) {
  audio_processing_ = std::move(audio_processing);
  task_queue_factory_ = std::move(task_queue_factory);
  audio_device_.swap(audio_device);
  encoder_factory_.swap(encoder_factory);
  decoder_factory_.swap(decoder_factory);

  process_thread_ = ProcessThread::Create("ModuleProcessThread");
  audio_mixer_ = AudioMixerImpl::Create();

  AudioProcessing::Config apm_config = audio_processing_->GetConfig();
  apm_config.echo_canceller.enabled = true;
  audio_processing_->ApplyConfig(apm_config);

  // AudioTransportImpl depends on audio mixer and audio processing instances.
  audio_transport_ = std::make_unique<AudioTransportImpl>(
      audio_mixer_.get(), audio_processing_.get());

  // Initialize ADM.
  if (audio_device_->Init() != 0) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the ADM.";
    return false;
  }

  // Initialize default speaker device.
  if (audio_device_->SetPlayoutDevice(AUDIO_DEVICE_ID) != 0) {
    RTC_LOG(LS_ERROR) << "Unable to set playout device.";
  }
  if (audio_device_->InitSpeaker() != 0) {
    RTC_LOG(LS_ERROR) << "Unable to access speaker.";
  }

  // Initialize default recording device.
  if (audio_device_->SetRecordingDevice(AUDIO_DEVICE_ID) != 0) {
    RTC_LOG(LS_ERROR) << "Unable to set recording device.";
  }
  if (audio_device_->InitMicrophone() != 0) {
    RTC_LOG(LS_ERROR) << "Unable to access microphone.";
  }

  // Set number of channels on speaker device.
  bool available = false;
  if (audio_device_->StereoPlayoutIsAvailable(&available) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to query stereo playout.";
  }
  if (audio_device_->SetStereoPlayout(available) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to set mono/stereo playout mode.";
  }

  // Set number of channels on recording device.
  available = false;
  if (audio_device_->StereoRecordingIsAvailable(&available) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to query stereo recording.";
  }
  if (audio_device_->SetStereoRecording(available) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to set mono/stereo recording mode.";
  }

  if (audio_device_->RegisterAudioCallback(audio_transport_.get()) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to register audio callback.";
  }

  return true;
}

absl::optional<ChannelId> VoipCore::CreateChannel(
    Transport* transport,
    absl::optional<uint32_t> local_ssrc) {
  absl::optional<ChannelId> channel;

  // Set local ssrc to random if not set by caller.
  if (!local_ssrc) {
    Random random(rtc::TimeMicros());
    local_ssrc = random.Rand<uint32_t>();
  }

  rtc::scoped_refptr<AudioChannel> audio_channel =
      new rtc::RefCountedObject<AudioChannel>(
          transport, local_ssrc.value(), task_queue_factory_.get(),
          process_thread_.get(), audio_mixer_.get(), decoder_factory_);

  {
    rtc::CritScope lock(&lock_);

    // Select index of vector that audio channel will be placed in.
    if (!idle_id_.empty()) {
      channel = idle_id_.front();
      idle_id_.pop();
      int index = static_cast<int>(*channel);
      // Existing id in idle queue guarantees channel vector boundary.
      channels_[index] = audio_channel;
    } else {
      // No more idle id to reuse, add new vector element.
      channel = static_cast<ChannelId>(channels_.size());
      channels_.push_back(audio_channel);
    }

    // Set ChannelId in audio channel for logging/debugging purpose.
    audio_channel->SetId(*channel);
  }

  return channel;
}

void VoipCore::ReleaseChannel(ChannelId channel) {
  // Destroy channel outside of the lock.
  rtc::scoped_refptr<AudioChannel> audio_channel;
  {
    rtc::CritScope lock(&lock_);
    int index = static_cast<int>(channel);
    if (index < 0 || index >= static_cast<int>(channels_.size())) {
      RTC_LOG(LS_ERROR) << "channel out of range " << index;
      return;
    }
    audio_channel.swap(channels_[index]);
    if (audio_channel) {
      idle_id_.push(channel);
    }
  }
}

rtc::scoped_refptr<AudioChannel> VoipCore::GetChannel(ChannelId channel) {
  rtc::CritScope lock(&lock_);
  int index = static_cast<int>(channel);
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    RTC_LOG(LS_ERROR) << "channel out of range " << index;
    return nullptr;
  }
  return channels_[index];
}

bool VoipCore::UpdateAudioTransportWithSenders() {
  std::vector<AudioSender*> audio_senders;

  // Gather a list of audio channel that are currently sending along with
  // highest sampling rate and channel numbers to configure into audio
  // transport.
  {
    int max_sampling_rate = 8000;
    size_t max_num_channels = 1;

    rtc::CritScope lock(&lock_);
    for (auto& channel : channels_) {
      if (channel->IsSendingMedia()) {
        auto encoder_format = channel->GetEncoderFormat();
        if (!encoder_format) {
          RTC_LOG(LS_ERROR)
              << "channel " << channel->GetId() << " encoder is not set";
          continue;
        }
        audio_senders.push_back(channel->GetAudioSender());
        max_sampling_rate =
            std::max(max_sampling_rate, encoder_format->clockrate_hz);
        max_num_channels =
            std::max(max_num_channels, encoder_format->num_channels);
      }
    }
    audio_transport_->UpdateAudioSenders(audio_senders, max_sampling_rate,
                                         max_num_channels);
  }

  // Depending on availability of senders, turn on or off ADM recording.
  if (!audio_senders.empty()) {
    if (!audio_device_->Recording()) {
      if (audio_device_->InitRecording() != 0) {
        RTC_LOG(LS_ERROR) << "InitRecording failed";
        return false;
      }
      if (audio_device_->StartRecording() != 0) {
        RTC_LOG(LS_ERROR) << "StartRecording failed";
        return false;
      }
    }
  } else {
    if (audio_device_->Recording()) {
      if (audio_device_->StopRecording() != 0) {
        RTC_LOG(LS_ERROR) << "StopRecording failed";
        return false;
      }
    }
  }
  return true;
}

bool VoipCore::StartSend(ChannelId channel) {
  auto audio_channel = GetChannel(channel);
  if (!audio_channel) {
    return false;
  }

  audio_channel->StartSend();

  return UpdateAudioTransportWithSenders();
}

bool VoipCore::StopSend(ChannelId channel) {
  auto audio_channel = GetChannel(channel);
  if (!audio_channel) {
    return false;
  }

  audio_channel->StopSend();

  return UpdateAudioTransportWithSenders();
}

bool VoipCore::StartPlayout(ChannelId channel) {
  auto audio_channel = GetChannel(channel);
  if (!audio_channel) {
    return false;
  }

  audio_channel->StartPlay();

  if (!audio_device_->Playing()) {
    if (audio_device_->InitPlayout() != 0) {
      RTC_LOG(LS_ERROR) << "InitPlayout failed";
      return false;
    }
    if (audio_device_->StartPlayout() != 0) {
      RTC_LOG(LS_ERROR) << "StartPlayout failed";
      return false;
    }
  }
  return true;
}

bool VoipCore::StopPlayout(ChannelId channel) {
  auto audio_channel = GetChannel(channel);
  if (!audio_channel) {
    return false;
  }

  audio_channel->StopPlay();

  bool stop_device = true;
  {
    rtc::CritScope lock(&lock_);
    for (auto channel : channels_) {
      if (channel->IsPlaying()) {
        stop_device = false;
        break;
      }
    }
  }

  if (stop_device && audio_device_->Playing()) {
    if (audio_device_->StopPlayout() != 0) {
      RTC_LOG(LS_ERROR) << "StopPlayout failed";
      return false;
    }
  }
  return true;
}

void VoipCore::ReceivedRTPPacket(ChannelId channel,
                                 rtc::ArrayView<const uint8_t> rtp) {
  if (auto audio_channel = GetChannel(channel)) {
    audio_channel->ReceivedRTPPacket(rtp);
  }
}

void VoipCore::ReceivedRTCPPacket(ChannelId channel,
                                  rtc::ArrayView<const uint8_t> rtcp) {
  if (auto audio_channel = GetChannel(channel)) {
    audio_channel->ReceivedRTCPPacket(rtcp);
  }
}

void VoipCore::SetSendCodec(ChannelId channel,
                            int payload_type,
                            const SdpAudioFormat& encoder_format) {
  if (auto audio_channel = GetChannel(channel)) {
    auto encoder = encoder_factory_->MakeAudioEncoder(
        payload_type, encoder_format, absl::nullopt);
    audio_channel->SetEncoder(payload_type, encoder_format, std::move(encoder));
  }
}

void VoipCore::SetReceiveCodecs(
    ChannelId channel,
    const std::map<int, SdpAudioFormat>& decoder_specs) {
  if (auto audio_channel = GetChannel(channel)) {
    audio_channel->SetReceiveCodecs(decoder_specs);
  }
}

}  // namespace webrtc
