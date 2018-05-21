/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_INPUT_WIN_H_
#define MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_INPUT_WIN_H_

#include <memory>
#include <string>

#include "api/optional.h"
#include "modules/audio_device/win/audio_device_module_win.h"
#include "modules/audio_device/win/core_audio_base_win.h"

namespace webrtc {

class AudioDeviceBuffer;
class FineAudioBuffer;

namespace webrtc_win {

class CoreAudioInput final : public CoreAudioBase,
                             public AudioInput,
                             public CoreAudioObserverInterface {
 public:
  CoreAudioInput();
  ~CoreAudioInput() override;

  // CoreAudioObserverInterface implementation.
  bool OnDataCallback(uint64_t device_frequency) override;

  // AudioInput implementation.
  int Init() override;
  int Terminate() override;
  int NumDevices() const override;
  int SetDevice(int index) override;
  int SetDevice(AudioDeviceModule::WindowsDeviceType device) override;
  int DeviceName(int index,
                 char name[kAdmMaxDeviceNameSize],
                 char guid[kAdmMaxGuidSize]) override;
  void AttachAudioBuffer(AudioDeviceBuffer* audio_buffer) override;
  bool RecordingIsInitialized() const override;
  int InitRecording() override;
  int StartRecording() override;
  int StopRecording() override;
  bool Recording() override;

  CoreAudioInput(const CoreAudioInput&) = delete;
  CoreAudioInput& operator=(const CoreAudioInput&) = delete;

 private:
  rtc::Optional<int> EstimateLatencyMillis(uint64_t capture_time_100ns);

  std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;
  Microsoft::WRL::ComPtr<IAudioCaptureClient> audio_capture_client_;
  rtc::Optional<double> qpc_to_100ns_;
};

}  // namespace webrtc_win

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_INPUT_WIN_H_
