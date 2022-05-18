/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "external_audio_device.h"

#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"

#include "sdk/objc/native/src/audio/audio_device_module_ios.h"
#include "sdk/objc/native/src/audio/objc_audio_device.h"

#include <functional>

namespace webrtc {

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
    id<RTC_OBJC_TYPE(RTCAudioDevice)> audio_device) {
  RTC_DLOG(LS_INFO) << __FUNCTION__;
#if defined(WEBRTC_IOS)
  std::function<std::unique_ptr<AudioDeviceGeneric>()> audio_device_factory = [audio_device]() {
    return std::make_unique<objc_adm::ObjCAudioDevice>(audio_device);
  };
  return rtc::make_ref_counted<ios_adm::AudioDeviceModuleIOS>(audio_device_factory);
#else
  RTC_LOG(LS_ERROR) << "current platform is not supported => this module will self destruct!";
  return nullptr;
#endif
}

}  // namespace webrtc
