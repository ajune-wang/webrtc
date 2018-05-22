/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_BASE_WIN_H_
#define MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_BASE_WIN_H_

#include <memory>
#include <string>

// #include "api/optional.h"
#include "modules/audio_device/win/core_audio_utility_win.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class AudioDeviceBuffer;
class FineAudioBuffer;

namespace webrtc_win {

class CoreAudioObserverInterface {
 public:
  virtual bool OnDataCallback(uint64_t device_frequency) = 0;

 protected:
  virtual ~CoreAudioObserverInterface() {}
};

class CoreAudioBase {
 public:
  enum class Direction {
    kInput,
    kOutput,
  };

  explicit CoreAudioBase(Direction direction,
                         CoreAudioObserverInterface* observer);
  ~CoreAudioBase();

  std::string GetDeviceID(int index) const;
  int DeviceName(int index,
                 char name[kAdmMaxDeviceNameSize],
                 char guid[kAdmMaxGuidSize]) const;

  bool Init();
  bool Start();
  bool Stop();

  Direction direction() const { return direction_; }
  CoreAudioObserverInterface* observer() const { return observer_; }

  CoreAudioBase(const CoreAudioBase&) = delete;
  CoreAudioBase& operator=(const CoreAudioBase&) = delete;

  void ThreadRun();

 protected:
  // Returns number of active devices given the specified |direction_|.
  int NumberOfActiveDevices() const;

  // Returns total number of enumerated audio devices which is the sum of all
  // active devices plus two extra (one default and one default
  // communications). The value in |direction_| determines if capture or
  // render devices are counted.
  int NumberOfEnumeratedDevices() const;

  bool IsInput() const;
  bool IsOutput() const;
  bool IsDefaultDevice(int index) const;
  bool IsDefaultCommunicationsDevice(int index) const;
  bool IsDefaultDevice(const std::string& device_id) const;
  bool IsDefaultCommunicationsDevice(const std::string& device_id) const;
  EDataFlow GetDataFlow() const;

  rtc::ThreadChecker thread_checker_;
  CoreAudioObserverInterface* observer_ = nullptr;
  AudioDeviceBuffer* audio_device_buffer_ = nullptr;
  Direction direction_;
  bool initialized_ = false;
  std::string device_id_;

  WAVEFORMATEXTENSIBLE format_ = {};
  uint32_t endpoint_buffer_size_frames_ = 0;
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
  Microsoft::WRL::ComPtr<IAudioClock> audio_clock_;
  ScopedHandle audio_samples_event_;
  ScopedHandle stop_event_;
  std::unique_ptr<rtc::PlatformThread> audio_thread_;

 private:
  void StopThread();
};

}  // namespace webrtc_win
}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_BASE_WIN_H_
