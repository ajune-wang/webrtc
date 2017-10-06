/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/audio_device_factory.h"

#include "modules/audio_device/audio_device_proxy.h"

// #include "rtc_base/checks.h"
// #include "rtc_base/logging.h"
// #include "rtc_base/refcountedobject.h"

namespace webrtc {

rtc::scoped_refptr<webrtc::AudioDeviceModule> AudioDeviceFactory::Create() {
  return AudioDeviceModule::Create(1, AudioDeviceModule::kPlatformDefaultAudio);
}

rtc::scoped_refptr<webrtc::AudioDeviceModule>
AudioDeviceFactory::CreateProxy() {
  return AudioDeviceProxy::Create(1, AudioDeviceModule::kPlatformDefaultAudio);
}

}  // namespace webrtc
