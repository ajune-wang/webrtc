/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_ANDROID_AAUDIO_WRAPPER_H_
#define MODULES_AUDIO_DEVICE_ANDROID_AAUDIO_WRAPPER_H_

#include <aaudio/AAudio.h>

#include "modules/audio_device/include/audio_device_defines.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class AudioManager;

// AAudio callback interface for audio transport to/from the audio stream.
// The interface also contains an erro callback method for notification of
// e.g. device changes.
class AAudioObserverInterface {
 public:
  virtual aaudio_data_callback_result_t OnDataCallback(void* audio_data,
                                                       int32_t num_frames) = 0;
  virtual void OnErrorCallback(aaudio_result_t error) = 0;

 protected:
  virtual ~AAudioObserverInterface() {}
};

// TOOD(henrika): add comments...
// These calls are thread safe:
//
// - AAudio_convert*ToText()
// - AAudio_createStreamBuilder()
// - AAudioStream_get*() except for AAudioStream_getTimestamp()
//
class AAudioWrapper {
 public:
  AAudioWrapper(AudioManager* audio_manager,
                aaudio_direction_t direction,
                AAudioObserverInterface* observer);
  ~AAudioWrapper();

  bool Init();
  bool Start();
  bool Stop();

  AAudioObserverInterface* observer() const { return observer_; }
  AudioParameters audio_parameters() const { return audio_parameters_; }
  int32_t samples_per_frame() const;
  int32_t buffer_size_in_frames() const;
  int32_t device_id() const;
  int32_t xrun_count() const;
  int32_t format() const;
  int32_t sample_rate() const;
  int32_t channel_count() const;
  int32_t frames_per_callback() const;
  aaudio_sharing_mode_t sharing_mode() const;
  aaudio_performance_mode_t performance_mode() const;
  aaudio_direction_t direction() const;

 private:
  bool CreateStreamBuilder();
  void DeleteStreamBuilder();
  void SetStreamConfiguration();
  void LogStreamConfiguration();
  bool OpenStream();
  void CloseStream();
  void LogStreamState();
  bool VerifyStreamConfiguration();
  bool OptimizeBuffers();

  rtc::ThreadChecker thread_checker_;
  AudioParameters audio_parameters_;
  aaudio_direction_t direction_;
  AAudioObserverInterface* observer_ = nullptr;
  AAudioStreamBuilder* builder_ = nullptr;
  AAudioStream* stream_ = nullptr;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_ANDROID_AAUDIO_WRAPPER_H_
