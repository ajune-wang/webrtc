/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/audio_device_module/audio_device_android.h"

#include "rtc_base/logging.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "system_wrappers/include/metrics.h"

#include <stdlib.h>
#include "sdk/android/src/jni/audio_device/audio_device_template_android.h"
#include "sdk/android/src/jni/audio_device/audio_manager.h"
#include "sdk/android/src/jni/audio_device/audio_record_jni.h"
#include "sdk/android/src/jni/audio_device/audio_track_jni.h"
#include "sdk/android/src/jni/audio_device/opensles_player.h"
#include "sdk/android/src/jni/audio_device/opensles_recorder.h"

namespace webrtc {

// static
rtc::scoped_refptr<AudioDeviceModule> AudioDeviceModuleAndroid::Create() {
  RTC_LOG(INFO) << __FUNCTION__;
  AudioDeviceModuleAndroid audio_device_android;
  rtc::scoped_refptr<AudioDeviceModule> audio_device;
  // Create the required objects.
  return audio_device_android.CreateObjects();
}

AudioDeviceModuleAndroid::AudioDeviceModuleAndroid() {
  RTC_LOG(INFO) << __FUNCTION__;
}

AudioDeviceModuleAndroid::~AudioDeviceModuleAndroid() {
  RTC_LOG(INFO) << __FUNCTION__;
}

rtc::scoped_refptr<AudioDeviceModule> AudioDeviceModuleAndroid::CreateObjects() {
  RTC_LOG(INFO) << __FUNCTION__;
  AudioDeviceModule::AudioLayer audio_layer((
      AudioDeviceModule::AudioLayer::kPlatformDefaultAudio));
  // Create an Android audio manager.
  std::unique_ptr<AudioManager> audio_manager_android = rtc::MakeUnique<AudioManager>();
  // Select best possible combination of audio layers.
  if (audio_layer == AudioDeviceModule::kPlatformDefaultAudio) {
    if (audio_manager_android->IsLowLatencyPlayoutSupported() &&
        audio_manager_android->IsLowLatencyRecordSupported()) {
      // Use OpenSL ES for both playout and recording.
      audio_layer = AudioDeviceModule::kAndroidOpenSLESAudio;
    } else if (audio_manager_android->IsLowLatencyPlayoutSupported() &&
               !audio_manager_android->IsLowLatencyRecordSupported()) {
      // Use OpenSL ES for output on devices that only supports the
      // low-latency output audio path.
      audio_layer = AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio;
    } else {
      // Use Java-based audio in both directions when low-latency output is
      // not supported.
      audio_layer = AudioDeviceModule::kAndroidJavaAudio;
    }
  }
  rtc::scoped_refptr<AudioDeviceModule> audio_device;
  if (audio_layer == AudioDeviceModule::kAndroidJavaAudio) {
    // Java audio for both input and output audio.
    audio_device =
        new rtc::RefCountedObject<AudioDeviceTemplateAndroid<AudioRecordJni, AudioTrackJni>>(
            audio_layer, std::move(audio_manager_android));
  } else if (audio_layer == AudioDeviceModule::kAndroidOpenSLESAudio) {
    // OpenSL ES based audio for both input and output audio.
    audio_device =
        new rtc::RefCountedObject<AudioDeviceTemplateAndroid<OpenSLESRecorder, OpenSLESPlayer>>(
            audio_layer, std::move(audio_manager_android));
  } else if (
      audio_layer == AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio) {
    // Java audio for input and OpenSL ES for output audio (i.e. mixed APIs).
    // This combination provides low-latency output audio and at the same
    // time support for HW AEC using the AudioRecord Java API.
    audio_device =
        new rtc::RefCountedObject<AudioDeviceTemplateAndroid<AudioRecordJni, OpenSLESPlayer>>(
            audio_layer, std::move(audio_manager_android));
  } else {
    // Invalid audio layer.
    audio_device = nullptr;
  }

  if (!audio_device) {
    RTC_LOG(LS_ERROR) << "Failed to create the ADM implementation.";
  }
  return audio_device;
}

}  // namespace webrtc
