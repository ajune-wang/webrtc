/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_NATIVE_API_AUDIO_DEVICE_MODULE_AUDIO_DEVICE_ANDROID_H_
#define SDK_ANDROID_NATIVE_API_AUDIO_DEVICE_MODULE_AUDIO_DEVICE_ANDROID_H_

#include <jni.h>

#include "modules/audio_device/include/audio_device.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc {

class AudioDeviceModuleBuilder {
 public:
  AudioDeviceModuleBuilder(JNIEnv* env, jobject application_context);

  // AudioDeviceModuleBuilder& Create(JNIEnv* env,
  //    jobject application_context);

  AudioDeviceModuleBuilder& SetStereoInput(bool use_stereo_input);
  AudioDeviceModuleBuilder& SetStereoOutput(bool use_stereo_output);
  AudioDeviceModuleBuilder& SetInputSampleRate(int input_sample_rate);
  AudioDeviceModuleBuilder& SetOutputSampleRate(int output_sample_rate);
  AudioDeviceModuleBuilder& SetUsageAttribute(int usage_attribute);

#if defined(WEBRTC_AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
  rtc::scoped_refptr<AudioDeviceModule> BuildAAudioDeviceModule();
#endif
  rtc::scoped_refptr<AudioDeviceModule> BuildJavaAudioDeviceModule();
  rtc::scoped_refptr<AudioDeviceModule> BuildOpenSLESAudioDeviceModule();
  rtc::scoped_refptr<AudioDeviceModule>
  BuildJavaInputAndOpenSLESOutputAudioDeviceModule();

  JNIEnv* env_;
  ScopedJavaLocalRef<jobject> j_context_;
  ScopedJavaLocalRef<jobject> j_audio_manager_;
  bool use_stereo_input_;
  bool use_stereo_output_;
  int usage_attribute_;
  AudioParameters input_parameters_;
  AudioParameters output_parameters_;
};

#if defined(WEBRTC_AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
rtc::scoped_refptr<AudioDeviceModule> CreateAAudioAudioDeviceModule(
    JNIEnv* env,
    jobject application_context);
#endif

rtc::scoped_refptr<AudioDeviceModule> CreateJavaAudioDeviceModule(
    JNIEnv* env,
    jobject application_context);

rtc::scoped_refptr<AudioDeviceModule> CreateOpenSLESAudioDeviceModule(
    JNIEnv* env,
    jobject application_context);

rtc::scoped_refptr<AudioDeviceModule>
CreateJavaInputAndOpenSLESOutputAudioDeviceModule(JNIEnv* env,
                                                  jobject application_context);

}  // namespace webrtc

#endif  // SDK_ANDROID_NATIVE_API_AUDIO_DEVICE_MODULE_AUDIO_DEVICE_ANDROID_H_
