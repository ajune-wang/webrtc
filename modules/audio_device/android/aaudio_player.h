/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_ANDROID_AAUDIO_PLAYER_H_
#define MODULES_AUDIO_DEVICE_ANDROID_AAUDIO_PLAYER_H_

#include <aaudio/AAudio.h>
#include <memory>

// #include "modules/audio_device/android/audio_common.h"
// #include "modules/audio_device/android/audio_manager.h"
// #include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/android/aaudio_wrapper.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class AudioDeviceBuffer;
class FineAudioBuffer;
class AudioManager;

class AAudioPlayer : public AAudioObserverInterface {
 public:
  explicit AAudioPlayer(AudioManager* audio_manager);
  ~AAudioPlayer();

  int Init();
  int Terminate();

  int InitPlayout();
  bool PlayoutIsInitialized() const { return initialized_; }

  int StartPlayout();
  int StopPlayout();
  bool Playing() const { return playing_; }

  int SpeakerVolumeIsAvailable(bool& available) { return -1; }
  int SetSpeakerVolume(uint32_t volume) { return -1; }
  int SpeakerVolume(uint32_t& volume) const { return -1; }
  int MaxSpeakerVolume(uint32_t& maxVolume) const { return -1; }
  int MinSpeakerVolume(uint32_t& minVolume) const { return -1; }

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

  // AAudioObserverInterface implementation.

  // For an output stream, this function should render and write |num_frames|
  // of data in the streams current data format to the |audio_data| buffer.
  // Note that this callback function will be called on a real-time thread
  // owned by AAudio
  aaudio_data_callback_result_t OnDataCallback(void* audio_data,
                                               int32_t num_frames);
  void OnErrorCallback(aaudio_result_t error);

 private:
  // Ensures that methods are called from the same thread as this object is
  // created on.
  rtc::ThreadChecker thread_checker_;

  // Stores thread ID in first call to AAudioPlayer::OnDataCallback from a
  // real-time thread owned by AAudio. Detached during construction of this
  // object.
  rtc::ThreadChecker thread_checker_aaudio_;

  AAudioWrapper aaudio_;

  // Raw pointer to the audio manager injected at construction. Used to cache
  // audio parameters and to access the global SL engine object needed by the
  // ObtainEngineInterface() method. The audio manager outlives any instance of
  // this class.
  // AudioManager* audio_manager_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModule::Create().
  AudioDeviceBuffer* audio_device_buffer_ = nullptr;

  bool initialized_ = false;
  bool playing_ = false;

  int32_t underrun_count_ = 0;

  // FineAudioBuffer takes an AudioDeviceBuffer which delivers audio data
  // in chunks of 10ms. It then allows for this data to be pulled in
  // a finer or coarser granularity. I.e. interacting with this class instead
  // of directly with the AudioDeviceBuffer one can ask for any number of
  // audio data samples.
  // Example: native buffer size can be 192 audio frames at 48kHz sample rate.
  // WebRTC will provide 480 audio frames per 10ms but AAudio asks for 192
  // in each callback (one every 4th ms). This class can then ask for 192 and
  // the FineAudioBuffer will ask WebRTC for new data approximately only every
  // second callback and also cache non-utilized audio.
  std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;

  uint32_t last_play_time_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_ANDROID_AAUDIO_PLAYER_H_
