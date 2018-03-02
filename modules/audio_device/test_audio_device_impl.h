/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_DEVICE_TEST_AUDIO_DEVICE_IMPL_H_
#define MODULES_AUDIO_DEVICE_TEST_AUDIO_DEVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/buffer.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/random.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class EventTimerWrapper;

// TestAudioDeviceModule implements an AudioDevice module that can act both as a
// capturer and a renderer. It will use 10ms audio frames.
class TestAudioDeviceModuleImpl : public TestAudioDeviceModule {
 public:
  // A fake capturer that generates pulses with random samples between
  // -max_amplitude and +max_amplitude.
  class PulsedNoiseCapturerImpl final : public PulsedNoiseCapturer {
   public:
    PulsedNoiseCapturerImpl(int16_t max_amplitude,
                            int sampling_frequency_in_hz);

    int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

    bool Capture(rtc::BufferT<int16_t>* buffer) override;

    void SetMaxAmplitude(int16_t amplitude) override;

   private:
    int sampling_frequency_in_hz_;
    bool fill_with_zero_;
    Random random_generator_;
    rtc::CriticalSection lock_;
    int16_t max_amplitude_ RTC_GUARDED_BY(lock_);
  };

  // Creates a new TestAudioDeviceModule. When capturing or playing, 10 ms audio
  // frames will be processed every 10ms / |speed|.
  // |capturer| is an object that produces audio data. Can be nullptr if this
  // device is never used for recording.
  // |renderer| is an object that receives audio data that would have been
  // played out. Can be nullptr if this device is never used for playing.
  // Use one of the Create... functions to get these instances.
  TestAudioDeviceModuleImpl(std::unique_ptr<Capturer> capturer,
                            std::unique_ptr<Renderer> renderer,
                            float speed);
  ~TestAudioDeviceModuleImpl() override;

  int32_t Init() override;
  int32_t RegisterAudioCallback(AudioTransport* callback) override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;

  bool Playing() const override;
  bool Recording() const override;

  // Blocks until the Renderer refuses to receive data.
  // Returns false if |timeout_ms| passes before that happens.
  bool WaitForPlayoutEnd(int timeout_ms = rtc::Event::kForever) override;
  // Blocks until the Recorder stops producing data.
  // Returns false if |timeout_ms| passes before that happens.
  bool WaitForRecordingEnd(int timeout_ms = rtc::Event::kForever) override;

 private:
  static bool Run(void* obj);
  void ProcessAudio();

  const std::unique_ptr<Capturer> capturer_ RTC_GUARDED_BY(lock_);
  const std::unique_ptr<Renderer> renderer_ RTC_GUARDED_BY(lock_);
  const float speed_;

  rtc::CriticalSection lock_;
  AudioTransport* audio_callback_ RTC_GUARDED_BY(lock_);
  bool rendering_ RTC_GUARDED_BY(lock_);
  bool capturing_ RTC_GUARDED_BY(lock_);
  rtc::Event done_rendering_;
  rtc::Event done_capturing_;

  std::vector<int16_t> playout_buffer_ RTC_GUARDED_BY(lock_);
  rtc::BufferT<int16_t> recording_buffer_ RTC_GUARDED_BY(lock_);

  std::unique_ptr<EventTimerWrapper> tick_;
  rtc::PlatformThread thread_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_TEST_AUDIO_DEVICE_IMPL_H_
