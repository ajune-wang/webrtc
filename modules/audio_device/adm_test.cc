/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "modules/audio_device/audio_device_factory.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/sleep.h"

using webrtc::AudioDeviceModule;
using webrtc::SleepMs;

#define ENABLE_DEBUG_PRINTF
#ifdef ENABLE_DEBUG_PRINTF
#define logd(...) fprintf(stderr, __VA_ARGS__);
#else
#define logd(...) ((void)0)
#endif
#define log(...) fprintf(stderr, __VA_ARGS__);

int main(int /*argc*/, char** /*argv*/) {
  rtc::scoped_refptr<AudioDeviceModule> adm =
      webrtc::AudioDeviceFactory::CreateProxy();

  // rtc::scoped_refptr<AudioDeviceModule> adm =
  //    webrtc::AudioDeviceFactory::Create();

  adm->Init();
  adm->SetPlayoutDevice(0);
  adm->InitSpeaker();
  adm->SetRecordingDevice(0);
  adm->InitMicrophone();
  adm->SetStereoPlayout(true);
  adm->SetStereoRecording(false);
  adm->SetAGC(false);

  adm->InitPlayout();
  adm->StartPlayout();
  SleepMs(1000);
  adm->StopPlayout();

  adm->Terminate();

  return 0;
}
