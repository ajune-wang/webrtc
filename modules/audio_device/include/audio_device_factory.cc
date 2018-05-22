/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/include/audio_device_factory.h"

#if defined(WEBRTC_WIN)
#include "modules/audio_device/win/audio_device_module_win.h"
#include "modules/audio_device/win/core_audio_input_win.h"
#include "modules/audio_device/win/core_audio_output_win.h"
#endif

#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

rtc::scoped_refptr<AudioDeviceModule>
CreateWindowsCoreAudioAudioDeviceModule() {
  RTC_DLOG(INFO) << __FUNCTION__;
  return CreateWindowsCoreAudioAudioDeviceModuleFromInputAndOutput(
      AudioDeviceModule::kWindowsCoreAudio2,
      rtc::MakeUnique<webrtc_win::CoreAudioInput>(),
      rtc::MakeUnique<webrtc_win::CoreAudioOutput>());
}

}  // namespace webrtc
