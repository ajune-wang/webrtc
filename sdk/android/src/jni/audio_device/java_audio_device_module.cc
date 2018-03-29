/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/generated_peerconnection_jni/jni/JavaAudioDeviceModule_jni.h"

#include "sdk/android/src/jni/audio_device/audio_record_jni.h"
#include "sdk/android/src/jni/audio_device/audio_track_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static jlong JNI_JavaAudioDeviceModule_CreateAudioDeviceModule(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_context,
    const JavaParamRef<jobject>& j_audio_input,
    const JavaParamRef<jobject>& j_audio_output,
    int sample_rate,
    jboolean j_use_stereo_input,
    jboolean j_use_stereo_output) {
  AudioParameters input_parameters;
  AudioParameters output_parameters;
  android_adm::GetAudioParameters(env, j_context, sample_rate,
                                  j_use_stereo_input, j_use_stereo_output,
                                  &input_parameters, &output_parameters);
  auto audio_input = rtc::MakeUnique<android_adm::AudioRecordJni>(
      input_parameters,
      android_adm::kHighLatencyModeDelayEstimateInMilliseconds, j_audio_input);
  auto audio_output = rtc::MakeUnique<android_adm::AudioTrackJni>(
      output_parameters, j_audio_output);
  return jlongFromPointer(
      CreateAudioDeviceModuleFromInputAndOutput(
          AudioDeviceModule::kAndroidJavaAudio, j_use_stereo_input,
          j_use_stereo_output,
          android_adm::kHighLatencyModeDelayEstimateInMilliseconds,
          std::move(audio_input), std::move(audio_output))
          .release());
}

}  // namespace jni
}  // namespace webrtc
