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

#include <stdlib.h>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"

#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
#include "sdk/android/src/jni/audio_device/aaudio_player.h"
#include "sdk/android/src/jni/audio_device/aaudio_recorder.h"
#endif

#include "sdk/android/src/jni/application_context_provider.h"
#include "sdk/android/src/jni/audio_device/audio_record_jni.h"
#include "sdk/android/src/jni/audio_device/audio_track_jni.h"
#include "sdk/android/src/jni/audio_device/opensles_player.h"
#include "sdk/android/src/jni/audio_device/opensles_recorder.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

void GetDefaultAudioParameters(JNIEnv* env,
                               jobject application_context,
                               AudioParameters* input_parameters,
                               AudioParameters* output_parameters) {
  const JavaParamRef<jobject> j_context(application_context);
  const ScopedJavaLocalRef<jobject> j_audio_manager =
      jni::GetAudioManager(env, j_context);
  const int sample_rate = jni::GetDefaultSampleRate(env, j_audio_manager);
  jni::GetAudioParameters(env, j_context, j_audio_manager, sample_rate,
                          false /* use_stereo_input */,
                          false /* use_stereo_output */, input_parameters,
                          output_parameters);
}

AudioDeviceModule::AudioLayer GetDefaultAudioLayer(
    JNIEnv* env,
    jobject application_context) {
  const JavaParamRef<jobject> j_context(application_context);
  const ScopedJavaLocalRef<jobject> j_audio_manager =
      jni::GetAudioManager(env, j_context);
  if (jni::IsLowLatencyOutputSupported(env, j_audio_manager) &&
      jni::IsLowLatencyInputSupported(env, j_audio_manager)) {
    // Use OpenSL ES for both playout and recording.
    return AudioDeviceModule::kAndroidOpenSLESAudio;
  } else if (jni::IsLowLatencyOutputSupported(env, j_audio_manager) &&
             !jni::IsLowLatencyInputSupported(env, j_audio_manager)) {
    // Use OpenSL ES for output on devices that only supports the
    // low-latency output audio path.
    return AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio;
  } else {
    // Use Java-based audio in both directions when low-latency output is
    // not supported.
    return AudioDeviceModule::kAndroidJavaAudio;
  }
}

}  // namespace

#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
rtc::scoped_refptr<AudioDeviceModule> CreateAAudioAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  RTC_LOG(INFO) << __FUNCTION__;
  // Get default audio input/output parameters.
  AudioParameters input_parameters;
  AudioParameters output_parameters;
  GetDefaultAudioParameters(env, application_context, &input_parameters,
                            &output_parameters);
  // Create ADM from AAudioRecorder and AAudioPlayer.
  return CreateAudioDeviceModuleFromInputAndOutput(
      AudioDeviceModule::kAndroidAAudioAudio, false /* use_stereo_input */,
      false /* use_stereo_output */,
      jni::kLowLatencyModeDelayEstimateInMilliseconds,
      absl::make_unique<jni::AAudioRecorder>(input_parameters),
      absl::make_unique<jni::AAudioPlayer>(output_parameters));
}

rtc::scoped_refptr<AudioDeviceModule>
CreateJavaInputAndAAudioOutputAudioDeviceModule(JNIEnv* env,
                                                jobject application_context) {
  RTC_LOG(INFO) << __FUNCTION__;
  const JavaParamRef<jobject> j_context(application_context);
  const ScopedJavaLocalRef<jobject> j_audio_manager =
      jni::GetAudioManager(env, j_context);
  // Get default audio input/output parameters.
  AudioParameters input_parameters;
  AudioParameters output_parameters;
  GetDefaultAudioParameters(env, application_context, &input_parameters,
                            &output_parameters);
  auto audio_input = absl::make_unique<jni::AudioRecordJni>(
      env, input_parameters, jni::kHighLatencyModeDelayEstimateInMilliseconds,
      jni::AudioRecordJni::CreateJavaWebRtcAudioRecord(env, j_context,
                                                       j_audio_manager));
  // Create ADM from AAudioRecorder and AAudioPlayer.
  return CreateAudioDeviceModuleFromInputAndOutput(
      AudioDeviceModule::kAndroidAAudioAudio, false /* use_stereo_input */,
      false /* use_stereo_output */,
      jni::kLowLatencyModeDelayEstimateInMilliseconds, auido_input,
      absl::make_unique<jni::AAudioPlayer>(output_parameters));
}
#endif

rtc::scoped_refptr<AudioDeviceModule> CreateJavaAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  RTC_LOG(INFO) << __FUNCTION__;
  // Get default audio input/output parameters.
  const JavaParamRef<jobject> j_context(application_context);
  const ScopedJavaLocalRef<jobject> j_audio_manager =
      jni::GetAudioManager(env, j_context);
  AudioParameters input_parameters;
  AudioParameters output_parameters;
  GetDefaultAudioParameters(env, application_context, &input_parameters,
                            &output_parameters);
  // Create ADM from AudioRecord and AudioTrack.
  auto audio_input = absl::make_unique<jni::AudioRecordJni>(
      env, input_parameters, jni::kHighLatencyModeDelayEstimateInMilliseconds,
      jni::AudioRecordJni::CreateJavaWebRtcAudioRecord(env, j_context,
                                                       j_audio_manager));
  auto audio_output = absl::make_unique<jni::AudioTrackJni>(
      env, output_parameters,
      jni::AudioTrackJni::CreateJavaWebRtcAudioTrack(env, j_context,
                                                     j_audio_manager));
  return CreateAudioDeviceModuleFromInputAndOutput(
      AudioDeviceModule::kAndroidJavaAudio, false /* use_stereo_input */,
      false /* use_stereo_output */,
      jni::kHighLatencyModeDelayEstimateInMilliseconds, std::move(audio_input),
      std::move(audio_output));
}

rtc::scoped_refptr<AudioDeviceModule> CreateOpenSLESAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  RTC_LOG(INFO) << __FUNCTION__;
  // Get default audio input/output parameters.
  AudioParameters input_parameters;
  AudioParameters output_parameters;
  GetDefaultAudioParameters(env, application_context, &input_parameters,
                            &output_parameters);
  // Create ADM from OpenSLESRecorder and OpenSLESPlayer.
  auto engine_manager = absl::make_unique<jni::OpenSLEngineManager>();
  auto audio_input = absl::make_unique<jni::OpenSLESRecorder>(
      input_parameters, engine_manager.get());
  auto audio_output = absl::make_unique<jni::OpenSLESPlayer>(
      output_parameters, std::move(engine_manager));
  return CreateAudioDeviceModuleFromInputAndOutput(
      AudioDeviceModule::kAndroidOpenSLESAudio, false /* use_stereo_input */,
      false /* use_stereo_output */,
      jni::kLowLatencyModeDelayEstimateInMilliseconds, std::move(audio_input),
      std::move(audio_output));
}

rtc::scoped_refptr<AudioDeviceModule>
CreateJavaInputAndOpenSLESOutputAudioDeviceModule(JNIEnv* env,
                                                  jobject application_context) {
  RTC_LOG(INFO) << __FUNCTION__;
  // Get default audio input/output parameters.
  const JavaParamRef<jobject> j_context(application_context);
  const ScopedJavaLocalRef<jobject> j_audio_manager =
      jni::GetAudioManager(env, j_context);
  AudioParameters input_parameters;
  AudioParameters output_parameters;
  GetDefaultAudioParameters(env, application_context, &input_parameters,
                            &output_parameters);
  // Create ADM from AudioRecord and OpenSLESPlayer.
  auto audio_input = absl::make_unique<jni::AudioRecordJni>(
      env, input_parameters, jni::kLowLatencyModeDelayEstimateInMilliseconds,
      jni::AudioRecordJni::CreateJavaWebRtcAudioRecord(env, j_context,
                                                       j_audio_manager));
  auto audio_output = absl::make_unique<jni::OpenSLESPlayer>(
      output_parameters, absl::make_unique<jni::OpenSLEngineManager>());
  return CreateAudioDeviceModuleFromInputAndOutput(
      AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio,
      false /* use_stereo_input */, false /* use_stereo_output */,
      jni::kLowLatencyModeDelayEstimateInMilliseconds, std::move(audio_input),
      std::move(audio_output));
}

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModuleAndContext(
    AudioDeviceModule::AudioLayer audio_layer) {
  // Get JNIEnv and application context.
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> context = test::GetAppContextForTest(jni);
  // Select best possible combination of audio layers.
  if (audio_layer == AudioDeviceModule::kPlatformDefaultAudio) {
    audio_layer = GetDefaultAudioLayer(jni, context.obj());
  }
  if (audio_layer == AudioDeviceModule::kAndroidJavaAudio) {
    // Java audio for both input and output audio.
    return CreateJavaAudioDeviceModule(jni, context.obj());
  } else if (audio_layer == AudioDeviceModule::kAndroidOpenSLESAudio) {
    // OpenSL ES based audio for both input and output audio.
    return CreateOpenSLESAudioDeviceModule(jni, context.obj());
  } else if (audio_layer ==
             AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio) {
    // Java audio for input and OpenSL ES for output audio (i.e. mixed APIs).
    // This combination provides low-latency output audio and at the same
    // time support for HW AEC using the AudioRecord Java API.
    return CreateJavaInputAndOpenSLESOutputAudioDeviceModule(jni,
                                                             context.obj());
  } else if (audio_layer == AudioDeviceModule::kAndroidAAudioAudio) {
#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
    // AAudio based audio for both input and output.
    return CreateAAudioDeviceModule(jni, context.obj());
#endif
  } else if (audio_layer ==
             AudioDeviceModule::kAndroidJavaInputAndAAudioOutputAudio) {
#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
    // Java audio for input and AAudio for output audio (i.e. mixed APIs).
    return CreateJavaInputAndAAudioOutputAudioDeviceModule(jni, context.obj());
#endif
  }
  RTC_LOG(LS_ERROR) << "The requested audio layer is not supported";
  return nullptr;
}

}  // namespace webrtc
