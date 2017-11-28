/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_transport_proxy.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "audio/utility/audio_frame_operations.h"
#include "call/audio_send_stream.h"
#include "voice_engine/utility.h"

namespace webrtc {

namespace {
// Resample audio in |frame| to given sample rate preserving the
// channel count and place the result in |destination|.
int Resample(const AudioFrame& frame,
             const int destination_sample_rate,
             PushResampler<int16_t>* resampler,
             int16_t* destination) {
  const int number_of_channels = static_cast<int>(frame.num_channels_);
  const int target_number_of_samples_per_channel =
      destination_sample_rate / 100;
  resampler->InitializeIfNeeded(frame.sample_rate_hz_, destination_sample_rate,
                                number_of_channels);

  // TODO(yujo): make resampler take an AudioFrame, and add special case
  // handling of muted frames.
  return resampler->Resample(
      frame.data(), frame.samples_per_channel_ * number_of_channels,
      destination, number_of_channels * target_number_of_samples_per_channel);
}
}  // namespace

AudioTransportProxy::AudioTransportProxy(AudioDeviceModule* audio_device_module,
                                         AudioProcessing* audio_processing,
                                         AudioMixer* mixer)
    : audio_device_module_(audio_device_module),
      audio_processing_(audio_processing),
      mixer_(mixer) {
  RTC_DCHECK(audio_device_module);
  RTC_DCHECK(audio_processing);
  RTC_DCHECK(mixer);
}

AudioTransportProxy::~AudioTransportProxy() {}

// Not used in Chromium. Process captured audio and distribute to all sending
// streams. Try to do this at the lowest possible sample rate.
int32_t AudioTransportProxy::RecordedDataIsAvailable(
    const void* audio_data,
    const size_t number_of_frames,
    const size_t bytes_per_sample,
    const size_t number_of_channels,
    const uint32_t sample_rate,
    const uint32_t audio_delay_milliseconds,
    const int32_t clock_drift,
    const uint32_t volume,
    const bool key_pressed,
    uint32_t& new_mic_volume) {  // NOLINT: to avoid changing APIs
  RTC_DCHECK_EQ(2 * number_of_channels, bytes_per_sample);

  rtc::CritScope lock(&capture_lock_);

  std::unique_ptr<AudioFrame> audio_frame(new AudioFrame());

  // Resample input audio and create/store the initial audio frame
  // We want to process at the lowest rate possible without losing information.
  // Choose the lowest native rate at least equal to the input and codec rates.
  {
    int min_processing_rate_hz = std::min(static_cast<int>(sample_rate),
                                          send_sample_rate_hz_);
    for (int native_rate_hz : AudioProcessing::kNativeSampleRatesHz) {
      audio_frame->sample_rate_hz_ = native_rate_hz;
      if (audio_frame->sample_rate_hz_ >= min_processing_rate_hz) {
        break;
      }
    }
    audio_frame->num_channels_ = std::min(number_of_channels,
                                          send_num_channels_);
    voe::RemixAndResample(static_cast<const int16_t*>(audio_data),
                          number_of_frames, number_of_channels, sample_rate,
                          &capture_resampler_, audio_frame.get());
  }

  // Audio Processing.
  {
    int err = audio_processing_->set_stream_delay_ms(
        static_cast<uint16_t>(audio_delay_milliseconds));
    // Silently ignore this failure to avoid flooding the logs.

    GainControl* agc = audio_processing_->gain_control();
    err = agc->set_stream_analog_level(volume);
    RTC_DCHECK_EQ(0, err) << "set_stream_analog_level failed: "
                             "current_mic_level = " << volume;

    EchoCancellation* aec = audio_processing_->echo_cancellation();
    if (aec->is_drift_compensation_enabled()) {
      aec->set_stream_drift_samples(clock_drift);
    }

    audio_processing_->set_stream_key_pressed(key_pressed);

    err = audio_processing_->ProcessStream(audio_frame.get());
    RTC_DCHECK_EQ(0, err) << "ProcessStream() error: " << err;
  }

  {
    constexpr uint32_t kMaxVolumeLevel = 255;

    uint32_t max_volume = 0;
    uint16_t voe_mic_level = 0;
    // Check for zero to skip this calculation; the consumer may use this to
    // indicate no volume is available.
    if (volume != 0) {
      // Scale from ADM to VoE level range
      if (audio_device_module_->MaxMicrophoneVolume(&max_volume) == 0) {
        if (max_volume) {
          voe_mic_level = static_cast<uint16_t>(
              (volume * kMaxVolumeLevel + static_cast<int>(max_volume / 2)) /
              max_volume);
        }
      }
      // We learned that on certain systems (e.g Linux) the voe_mic_level
      // can be greater than the maxVolumeLevel therefore
      // we are going to cap the voe_mic_level to the maxVolumeLevel
      // and change the maxVolume to volume if it turns out that
      // the voe_mic_level is indeed greater than the maxVolumeLevel.
      if (voe_mic_level > kMaxVolumeLevel) {
        voe_mic_level = kMaxVolumeLevel;
        max_volume = volume;
      }

      // Get capture level. Only updated when analog AGC is enabled.
      uint32_t new_voe_mic_level =
          audio_processing_->gain_control()->stream_analog_level();
      if (new_voe_mic_level != voe_mic_level) {
        // We'll return new volume if AGC has changed it, scaled to ADM range.
        new_mic_volume = static_cast<int>((new_voe_mic_level * max_volume +
                                  static_cast<int>(kMaxVolumeLevel / 2)) /
                                  kMaxVolumeLevel);
      }
    }
  }

  // Only swap if we're using a stereo codec.
  if (swap_stereo_channels_ && send_num_channels_ == 2) {
    AudioFrameOperations::SwapStereoChannels(audio_frame.get());
  }

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
  // Annoying typing detection (utilizes the APM/VAD decision)
  // We let the VAD determine if we're using this feature or not.
  if (audio_frame->vad_activity_ != AudioFrame::kVadUnknown) {
    bool vad_active = audio_frame->vad_activity_ == AudioFrame::kVadActive;
    typing_noise_detected_ = typing_detection_.Process(key_pressed, vad_active);
  }
#endif

  // Measure audio level of speech after all processing.
  double sample_duration = static_cast<double>(number_of_frames) / sample_rate;
  audio_level_.ComputeLevel(*audio_frame.get(), sample_duration);

  // Copy frame and push to each sending stream. The copy is required since an
  // encoding task will be posted internally to each stream.
  RTC_DCHECK_GT(audio_frame->samples_per_channel_, 0);
  if (!sending_streams_.empty()) {
    auto it = sending_streams_.begin();
    while (++it != sending_streams_.end()) {
      std::unique_ptr<AudioFrame> audio_frame_copy(new AudioFrame());
      audio_frame_copy->CopyFrom(*audio_frame.get());
      it->stream->OnAudioData(std::move(audio_frame_copy));
    }
    sending_streams_.begin()->stream->OnAudioData(std::move(audio_frame));
  }

  return 0;
}

// Mix all received streams, feed the result to the AudioProcessing module, then
// resample the result to the requested output rate.
int32_t AudioTransportProxy::NeedMorePlayData(const size_t nSamples,
                                              const size_t nBytesPerSample,
                                              const size_t nChannels,
                                              const uint32_t samplesPerSec,
                                              void* audioSamples,
                                              size_t& nSamplesOut,
                                              int64_t* elapsed_time_ms,
                                              int64_t* ntp_time_ms) {
  RTC_DCHECK_EQ(sizeof(int16_t) * nChannels, nBytesPerSample);
  RTC_DCHECK_GE(nChannels, 1);
  RTC_DCHECK_LE(nChannels, 2);
  RTC_DCHECK_GE(
      samplesPerSec,
      static_cast<uint32_t>(AudioProcessing::NativeRate::kSampleRate8kHz));

  // 100 = 1 second / data duration (10 ms).
  RTC_DCHECK_EQ(nSamples * 100, samplesPerSec);
  RTC_DCHECK_LE(nBytesPerSample * nSamples * nChannels,
                AudioFrame::kMaxDataSizeBytes);

  mixer_->Mix(nChannels, &mixed_frame_);
  *elapsed_time_ms = mixed_frame_.elapsed_time_ms_;
  *ntp_time_ms = mixed_frame_.ntp_time_ms_;

  const auto error = audio_processing_->ProcessReverseStream(&mixed_frame_);
  RTC_DCHECK_EQ(error, AudioProcessing::kNoError);

  nSamplesOut = Resample(mixed_frame_, samplesPerSec, &render_resampler_,
                         static_cast<int16_t*>(audioSamples));
  RTC_DCHECK_EQ(nSamplesOut, nChannels * nSamples);
  return 0;
}

void AudioTransportProxy::PushCaptureData(int voe_channel,
                                          const void* audio_data,
                                          int bits_per_sample,
                                          int sample_rate,
                                          size_t number_of_channels,
                                          size_t number_of_frames) {
  // This is part of deprecated VoE interface operating on specific
  // VoE channels. It should not be used.
  RTC_NOTREACHED();
}

// Used by Chromium - same as NeedMorePlayData() but because Chrome has its
// own APM instance, does not call audio_processing_->ProcessReverseStream().
void AudioTransportProxy::PullRenderData(int bits_per_sample,
                                         int sample_rate,
                                         size_t number_of_channels,
                                         size_t number_of_frames,
                                         void* audio_data,
                                         int64_t* elapsed_time_ms,
                                         int64_t* ntp_time_ms) {
  RTC_DCHECK_EQ(bits_per_sample, 16);
  RTC_DCHECK_GE(number_of_channels, 1);
  RTC_DCHECK_LE(number_of_channels, 2);
  RTC_DCHECK_GE(sample_rate, AudioProcessing::NativeRate::kSampleRate8kHz);

  // 100 = 1 second / data duration (10 ms).
  RTC_DCHECK_EQ(number_of_frames * 100, sample_rate);

  // 8 = bits per byte.
  RTC_DCHECK_LE(bits_per_sample / 8 * number_of_frames * number_of_channels,
                AudioFrame::kMaxDataSizeBytes);
  mixer_->Mix(number_of_channels, &mixed_frame_);
  *elapsed_time_ms = mixed_frame_.elapsed_time_ms_;
  *ntp_time_ms = mixed_frame_.ntp_time_ms_;

  auto output_samples = Resample(mixed_frame_, sample_rate, &render_resampler_,
                                 static_cast<int16_t*>(audio_data));
  RTC_DCHECK_EQ(output_samples, number_of_channels * number_of_frames);
}

void AudioTransportProxy::SetSendingStream(AudioSendStream* stream,
                                           bool sending,
                                           int sample_rate_hz,
                                           size_t num_channels) {
  RTC_DCHECK(stream);
  rtc::CritScope lock(&capture_lock_);
  auto it = std::find_if(sending_streams_.begin(), sending_streams_.end(),
                         [stream](const SendingStream& sending_stream){
                           return sending_stream.stream == stream;
                         });
  if (sending) {
    if (it == sending_streams_.end()) {
      SendingStream s;
      s.stream = stream;
      s.sample_rate_hz = sample_rate_hz;
      s.num_channels = num_channels;
      sending_streams_.push_back(s);
    } else {
      it->sample_rate_hz = sample_rate_hz;
      it->num_channels = num_channels;
    }
  } else {
    if (it != sending_streams_.end()) {
      sending_streams_.erase(it);
      if (sending_streams_.empty()) {
        audio_level_.Clear();
      }
    }
  }

  int max_sample_rate_hz = 8000;
  size_t max_num_channels = 1;
  for (const SendingStream& s : sending_streams_) {
    max_sample_rate_hz = std::max(max_sample_rate_hz, s.sample_rate_hz);
    max_num_channels = std::max(max_num_channels, s.num_channels);
  }
  send_sample_rate_hz_ = max_sample_rate_hz;
  send_num_channels_ = max_num_channels;
}

void AudioTransportProxy::SetStereoChannelSwapping(bool enable) {
  rtc::CritScope lock(&capture_lock_);
  swap_stereo_channels_ = enable;
}

bool AudioTransportProxy::typing_noise_detected() const {
  rtc::CritScope lock(&capture_lock_);
  return typing_noise_detected_;
}
}  // namespace webrtc
