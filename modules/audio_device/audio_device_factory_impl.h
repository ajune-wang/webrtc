/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_IMPL_H_
#define MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_IMPL_H_

#if defined(WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE)

#include <memory>

#include "modules/audio_device/audio_device_factory.h"
#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

// Defined and will be used only on Android.
class AudioManager;

class AudioDeviceFactoryImpl : public AudioDeviceFactory {
 public:
  ~AudioDeviceFactoryImpl() override = default;

  std::unique_ptr<AudioDeviceGeneric> CreateAudioDevice(
      AudioDeviceModule::AudioLayer audio_layer,
      AudioManager* android_audio_manager) override;
};

}  // namespace webrtc

#endif  // defined(WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE)

#endif  // MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_IMPL_H_
