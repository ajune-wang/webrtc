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

// #include "modules/audio_device/android/audio_common.h"
// #include "modules/audio_device/android/audio_manager.h"
// #include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class AudioDeviceBuffer;
class FineAudioBuffer;
class AudioManager;

class AAudioPlayer {
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

  // TODO(henrika): add comments...
  void OnErrorCallback(AAudioStream* stream, aaudio_result_t error);

  // For an output stream, this function should render and write |num_frames|
  // of data in the streams current data format to the |audio_data| buffer.
  // Note that this callback function should be considered a "real-time"
  // function. It must not do anything that could cause an unbounded delay
  // because that can cause the audio to glitch or pop.
  aaudio_data_callback_result_t OnDataCallback(AAudioStream* stream,
                                               void* audio_data,
                                               int32_t num_frames);

 private:
  // Creates a StreamBuilder which can be used to open a stream.
  // Owner must release resources using AAudioStreamBuilder_delete().
  // TODO(henrika): move to common place when recording is supported as well.
  bool CreateStreamBuilder();

  // Deletes the resources associated with the StreamBuilder.
  void DeleteStreamBuilder();

  void SetStreamConfiguration();
  void LogStreamConfiguration();
  bool VerifyStreamConfiguration();

  bool OpenStream();
  void CloseStream();

  // Ensures that methods are called from the same thread as this object is
  // created on.
  rtc::ThreadChecker thread_checker_;

  // Stores thread ID in first call to SimpleBufferQueueCallback() from internal
  // non-application thread which is not attached to the Dalvik JVM.
  // Detached during construction of this object.
  // rtc::ThreadChecker thread_checker_opensles_;

  // Raw pointer to the audio manager injected at construction. Used to cache
  // audio parameters and to access the global SL engine object needed by the
  // ObtainEngineInterface() method. The audio manager outlives any instance of
  // this class.
  // AudioManager* audio_manager_;

  // Contains audio parameters provided to this class at construction by the
  // AudioManager.
  const AudioParameters audio_parameters_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModule::Create().
  AudioDeviceBuffer* audio_device_buffer_;

  bool initialized_;
  bool playing_;

  // TODO(henrika): add comments...
  AAudioStreamBuilder* builder_ = nullptr;
  AAudioStream* stream_ = nullptr;

  // TODO(henrika): check if needed or not.
  int32_t device_id_;

  // FineAudioBuffer takes an AudioDeviceBuffer which delivers audio data
  // in chunks of 10ms. It then allows for this data to be pulled in
  // a finer or coarser granularity. I.e. interacting with this class instead
  // of directly with the AudioDeviceBuffer one can ask for any number of
  // audio data samples.
  // Example: native buffer size can be 192 audio frames at 48kHz sample rate.
  // WebRTC will provide 480 audio frames per 10ms but OpenSL ES asks for 192
  // in each callback (one every 4th ms). This class can then ask for 192 and
  // the FineAudioBuffer will ask WebRTC for new data approximately only every
  // second callback and also cache non-utilized audio.
  // std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_ANDROID_AAUDIO_PLAYER_H_
