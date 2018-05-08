/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_WIN_AUDIO_DEVICE_MODULE_WIN_H_
#define MODULES_AUDIO_DEVICE_WIN_AUDIO_DEVICE_MODULE_WIN_H_

#include <memory>

#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

class AudioDeviceModule;
class AudioDeviceBuffer;

namespace win_adm {

class AudioInput {
 public:
  virtual ~AudioInput() {}

  virtual int Init() = 0;
  virtual int Terminate() = 0;
  virtual int NumDevices() const = 0;
  virtual int SetDevice(int index) = 0;
  virtual int SetDevice(AudioDeviceModule::WindowsDeviceType device) = 0;
  virtual int DeviceName(int index,
                         char name[kAdmMaxDeviceNameSize],
                         char guid[kAdmMaxGuidSize]) = 0;
  virtual void AttachAudioBuffer(AudioDeviceBuffer* audio_buffer) = 0;
};

class AudioOutput {
 public:
  virtual ~AudioOutput() {}

  virtual int Init() = 0;
  virtual int Terminate() = 0;
  virtual int NumDevices() const = 0;
  virtual int SetDevice(int index) = 0;
  virtual int SetDevice(AudioDeviceModule::WindowsDeviceType device) = 0;
  virtual int DeviceName(int index,
                         char name[kAdmMaxDeviceNameSize],
                         char guid[kAdmMaxGuidSize]) = 0;
  virtual void AttachAudioBuffer(AudioDeviceBuffer* audio_buffer) = 0;
  virtual bool PlayoutIsInitialized() const = 0;
  virtual int InitPlayout() = 0;
  virtual int StartPlayout() = 0;
  virtual int StopPlayout() = 0;
  virtual bool Playing() = 0;
};

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModuleFromInputAndOutput(
    AudioDeviceModule::AudioLayer audio_layer,
    std::unique_ptr<AudioInput> audio_input,
    std::unique_ptr<AudioOutput> audio_output);

}  // namespace win_adm

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_WIN_AUDIO_DEVICE_MODULE_WIN_H_
