/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_H_
#define MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_H_

#include <memory>

#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

// Defined and will be used only on Android.
class AudioManager;

class AudioDeviceFactory {
 public:
  virtual ~AudioDeviceFactory() = default;

  // Creates platform specific audio devices.
  // `android_audio_manager` will be non-null only for Android platform.
  virtual std::unique_ptr<AudioDeviceGeneric> CreateAudioDevice(
      AudioDeviceModule::AudioLayer audio_layer,
      AudioManager* android_audio_manager) = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_H_
