/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_state.h"

#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace internal {

AudioState::AudioState(const AudioState::Config& config)
    : config_(config),
      voe_base_(config.voice_engine),
      audio_transport_proxy_(config_.audio_device_module.get(),
                             config_.audio_processing.get(),
                             config_.audio_mixer) {
  process_thread_checker_.DetachFromThread();
  RTC_DCHECK(config_.audio_mixer);
}

AudioState::~AudioState() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

VoiceEngine* AudioState::voice_engine() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_.voice_engine;
}

rtc::scoped_refptr<AudioMixer> AudioState::mixer() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_.audio_mixer;
}

bool AudioState::typing_noise_detected() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return audio_transport_proxy_.typing_noise_detected();
}

void AudioState::SetSendingStream(webrtc::AudioSendStream* stream, bool sending,
                                  int sample_rate_hz, size_t num_channels) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_transport_proxy_.SetSendingStream(stream, sending, sample_rate_hz,
                                          num_channels);
}

void AudioState::SetPlayout(bool enabled) {
  RTC_LOG(INFO) << "SetPlayout(" << enabled << ")";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  const bool currently_enabled = (null_audio_poller_ == nullptr);
  if (enabled == currently_enabled) {
    return;
  }
  if (enabled) {
    null_audio_poller_.reset();
  }
  // Will stop/start playout of the underlying device, if necessary, and
  // remember the setting for when it receives subsequent calls of
  // StartPlayout.
  voe_base_->SetPlayout(enabled);
  if (!enabled) {
    null_audio_poller_ =
        rtc::MakeUnique<NullAudioPoller>(&audio_transport_proxy_);
  }
}

void AudioState::SetRecording(bool enabled) {
  RTC_LOG(INFO) << "SetRecording(" << enabled << ")";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(henrika): keep track of state as in SetPlayout().
  // Will stop/start recording of the underlying device, if necessary, and
  // remember the setting for when it receives subsequent calls of
  // StartPlayout.
  voe_base_->SetRecording(enabled);
}

AudioState::LevelStats AudioState::CurrentAudioLevel() const {
  const voe::AudioLevel& audio_level = audio_transport_proxy_.audio_level();
  LevelStats result;
  result.audio_level = audio_level.LevelFullRange();
  RTC_DCHECK_LE(0, result.audio_level);
  result.total_input_energy = audio_level.TotalEnergy();
  result.total_input_duration = audio_level.TotalDuration();
  result.quantized_audio_level = audio_level.Level();
  return result;
}

void AudioState::SetStereoChannelSwapping(bool enable) {
  audio_transport_proxy_.SetStereoChannelSwapping(enable);
}

// Reference count; implementation copied from rtc::RefCountedObject.
void AudioState::AddRef() const {
  rtc::AtomicOps::Increment(&ref_count_);
}

// Reference count; implementation copied from rtc::RefCountedObject.
rtc::RefCountReleaseStatus AudioState::Release() const {
  if (rtc::AtomicOps::Decrement(&ref_count_) == 0) {
    delete this;
    return rtc::RefCountReleaseStatus::kDroppedLastRef;
  }
  return rtc::RefCountReleaseStatus::kOtherRefsRemained;
}
}  // namespace internal

rtc::scoped_refptr<AudioState> AudioState::Create(
    const AudioState::Config& config) {
  return rtc::scoped_refptr<AudioState>(new internal::AudioState(config));
}
}  // namespace webrtc
