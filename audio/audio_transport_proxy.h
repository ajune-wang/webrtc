/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_TRANSPORT_PROXY_H_
#define AUDIO_AUDIO_TRANSPORT_PROXY_H_

#include <vector>

#include "api/audio/audio_mixer.h"
#include "common_audio/resampler/include/push_resampler.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/typing_detection.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread_annotations.h"
#include "voice_engine/audio_level.h"

#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)
#define WEBRTC_VOICE_ENGINE_TYPING_DETECTION 1
#else
#define WEBRTC_VOICE_ENGINE_TYPING_DETECTION 0
#endif

namespace webrtc {

class AudioSendStream;

class AudioTransportProxy : public AudioTransport {
 public:
  AudioTransportProxy(AudioDeviceModule* audio_device_module,
                      AudioProcessing* audio_processing,
                      AudioMixer* mixer);

  ~AudioTransportProxy() override;

  int32_t RecordedDataIsAvailable(const void* audioSamples,
                                  const size_t nSamples,
                                  const size_t nBytesPerSample,
                                  const size_t nChannels,
                                  const uint32_t samplesPerSec,
                                  const uint32_t totalDelayMS,
                                  const int32_t clockDrift,
                                  const uint32_t currentMicLevel,
                                  const bool keyPressed,
                                  uint32_t& newMicLevel) override;

  int32_t NeedMorePlayData(const size_t nSamples,
                           const size_t nBytesPerSample,
                           const size_t nChannels,
                           const uint32_t samplesPerSec,
                           void* audioSamples,
                           size_t& nSamplesOut,
                           int64_t* elapsed_time_ms,
                           int64_t* ntp_time_ms) override;

  void PushCaptureData(int voe_channel,
                       const void* audio_data,
                       int bits_per_sample,
                       int sample_rate,
                       size_t number_of_channels,
                       size_t number_of_frames) override;

  void PullRenderData(int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames,
                      void* audio_data,
                      int64_t* elapsed_time_ms,
                      int64_t* ntp_time_ms) override;

  void SetSendingStream(AudioSendStream* stream, bool sending,
                        int sample_rate_hz, size_t num_channels);
  void SetStereoChannelSwapping(bool enable);
  bool typing_noise_detected() const;
  const voe::AudioLevel& audio_level() const {
    return audio_level_;
  }

 private:
  AudioDeviceModule* audio_device_module_;
  AudioProcessing* audio_processing_;

  // Sending side.
  rtc::CriticalSection capture_lock_;
  struct SendingStream {
    AudioSendStream* stream = nullptr;
    int sample_rate_hz = 0;
    size_t num_channels = 0;
  };
  std::vector<SendingStream> sending_streams_ RTC_GUARDED_BY(capture_lock_);
  int send_sample_rate_hz_ RTC_GUARDED_BY(capture_lock_) = 8000;
  size_t send_num_channels_ RTC_GUARDED_BY(capture_lock_) = 1;
  bool typing_noise_detected_ RTC_GUARDED_BY(capture_lock_) = false;
  bool swap_stereo_channels_ RTC_GUARDED_BY(capture_lock_) = false;
  // Converts audio device input rate to processing rate (max rate of sending
  // streams).
  PushResampler<int16_t> capture_resampler_;
  voe::AudioLevel audio_level_;
#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
  TypingDetection typing_detection_;
#endif

  // Playing side.
  rtc::scoped_refptr<AudioMixer> mixer_;
  AudioFrame mixed_frame_;
  // Converts mixed audio to the audio device output rate.
  PushResampler<int16_t> render_resampler_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioTransportProxy);
};
}  // namespace webrtc

#endif  // AUDIO_AUDIO_TRANSPORT_PROXY_H_
