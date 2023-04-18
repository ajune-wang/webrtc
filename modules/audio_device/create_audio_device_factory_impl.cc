/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/audio_device_factory.h"
#include "modules/audio_device/audio_device_factory_impl.h"
#include "modules/audio_device/create_audio_device_factory.h"

namespace webrtc {

std::unique_ptr<AudioDeviceFactory> CreateAudioDeviceFactory() {
  return std::make_unique<AudioDeviceFactoryImpl>();
}

}  // namespace webrtc
