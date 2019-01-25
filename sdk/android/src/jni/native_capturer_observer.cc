/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/native_capturer_observer.h"

#include "api/video_track_source_proxy.h"
#include "rtc_base/logging.h"
#include "sdk/android/generated_video_jni/jni/NativeCapturerObserver_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/android_video_track_source.h"

namespace webrtc {
namespace jni {

namespace {
VideoRotation jintToVideoRotation(jint rotation) {
  RTC_DCHECK(rotation == 0 || rotation == 90 || rotation == 180 ||
             rotation == 270);
  return static_cast<VideoRotation>(rotation);
}
}  // namespace

ScopedJavaLocalRef<jobject> CreateJavaNativeCapturerObserver(
    JNIEnv* env,
    rtc::scoped_refptr<AndroidVideoTrackSource> native_source) {
  return Java_NativeCapturerObserver_Constructor(
      env, NativeToJavaPointer(native_source.release()));
}

static ScopedJavaLocalRef<jobject>
JNI_NativeCapturerObserver_GetFrameAdaptationParameters(JNIEnv* env,
                                                        jlong j_source,
                                                        jint width,
                                                        jint height,
                                                        jint j_rotation,
                                                        int64_t timestamp_ns) {
  absl::optional<AndroidVideoTrackSource::FrameAdaptationParameters>
      parameters =
          reinterpret_cast<AndroidVideoTrackSource*>(j_source)
              ->GetFrameAdaptationParameters(width, height, timestamp_ns,
                                             jintToVideoRotation(j_rotation));

  if (!parameters)
    return nullptr;

  return Java_FrameAdaptationParameters_Constructor(
      env, parameters->crop_x, parameters->crop_y, parameters->crop_width,
      parameters->crop_height, parameters->adapted_width,
      parameters->adapted_height, parameters->aligned_timestamp_ns);
}

static void JNI_NativeCapturerObserver_OnFrameCaptured(
    JNIEnv* jni,
    jlong j_source,
    jint j_rotation,
    jlong j_timestamp_ns,
    const JavaParamRef<jobject>& j_video_frame_buffer) {
  reinterpret_cast<AndroidVideoTrackSource*>(j_source)->OnFrameCaptured(
      jni, j_timestamp_ns, jintToVideoRotation(j_rotation),
      j_video_frame_buffer);
}

static void JNI_NativeCapturerObserver_CapturerStarted(
    JNIEnv* jni,
    jlong j_source,
    jboolean j_success) {
  RTC_LOG(LS_INFO) << "NativeCapturerObserver_nativeCapturerStarted";
  AndroidVideoTrackSource* source =
      reinterpret_cast<AndroidVideoTrackSource*>(j_source);
  source->SetState(j_success ? AndroidVideoTrackSource::SourceState::kLive
                             : AndroidVideoTrackSource::SourceState::kEnded);
}

static void JNI_NativeCapturerObserver_CapturerStopped(
    JNIEnv* jni,
    jlong j_source) {
  RTC_LOG(LS_INFO) << "NativeCapturerObserver_nativeCapturerStopped";
  AndroidVideoTrackSource* source =
      reinterpret_cast<AndroidVideoTrackSource*>(j_source);
  source->SetState(AndroidVideoTrackSource::SourceState::kEnded);
}

}  // namespace jni
}  // namespace webrtc
