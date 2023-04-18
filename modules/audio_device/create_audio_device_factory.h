/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_CREATE_AUDIO_DEVICE_FACTORY_H_
#define MODULES_AUDIO_DEVICE_CREATE_AUDIO_DEVICE_FACTORY_H_

#include <memory>

#include "modules/audio_device/audio_device_factory.h"

namespace webrtc {

std::unique_ptr<AudioDeviceFactory> CreateAudioDeviceFactory();

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_CREATE_AUDIO_DEVICE_FACTORY_H_
