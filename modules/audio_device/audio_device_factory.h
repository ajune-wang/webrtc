/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_H_
#define MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_H_

#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

class AudioDeviceFactory {
 public:
  static rtc::scoped_refptr<webrtc::AudioDeviceModule> Create();
  static rtc::scoped_refptr<webrtc::AudioDeviceModule> CreateProxy();
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_AUDIO_DEVICE_FACTORY_H_
