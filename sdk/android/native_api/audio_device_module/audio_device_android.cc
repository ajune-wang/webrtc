/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
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

rtc::scoped_refptr<AudioDeviceModule> CreateAndroidAudioDeviceModule() {
  RTC_LOG(INFO) << __FUNCTION__;
  // Create an Android audio manager.
  AudioManager audio_manager_android;
  // Select best possible combination of audio layers.
  if (audio_manager_android.IsLowLatencyPlayoutSupported() &&
      audio_manager_android.IsLowLatencyRecordSupported()) {
    // Use OpenSL ES for both playout and recording.
    return new rtc::RefCountedObject<
        AudioDeviceTemplateAndroid<OpenSLESRecorder, OpenSLESPlayer>>(
        AudioDeviceModule::kAndroidOpenSLESAudio);
  } else if (audio_manager_android.IsLowLatencyPlayoutSupported() &&
             !audio_manager_android.IsLowLatencyRecordSupported()) {
    // Use OpenSL ES for output on devices that only supports the
    // low-latency output audio path.
    // This combination provides low-latency output audio and at the same
    // time support for HW AEC using the AudioRecord Java API.
    return new rtc::RefCountedObject<
        AudioDeviceTemplateAndroid<AudioRecordJni, OpenSLESPlayer>>(
        AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio);
  } else {
    // Use Java-based audio in both directions when low-latency output is
    // not supported.
    return new rtc::RefCountedObject<
        AudioDeviceTemplateAndroid<AudioRecordJni, AudioTrackJni>>(
        AudioDeviceModule::kAndroidJavaAudio);
  }
}

}  // namespace webrtc
