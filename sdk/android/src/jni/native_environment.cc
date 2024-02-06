/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "sdk/android/generated_native_environment_jni/NativeEnvironment_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc::jni {

jlong JNI_NativeEnvironment_CreateDefaultEnvironment(JNIEnv* env) {
  return NativeToJavaPointer(new Environment(CreateEnvironment()));
}

void JNI_NativeEnvironment_DeleteEnvironment(JNIEnv* env, jlong j_webrtc_env) {
  delete reinterpret_cast<Environment*>(j_webrtc_env);
}

}  // namespace webrtc::jni
