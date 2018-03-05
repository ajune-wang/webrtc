/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_NATIVE_API_SDK_JNI_H_
#define SDK_ANDROID_NATIVE_API_SDK_JNI_H_

#include <jni.h>

namespace webrtc {

// Entry point for clients that are using Java SDK but want to implement their
// own shared library with multiple libraries combined. This should be called
// from JNI_OnLoad.
jint OnJniLoad(JavaVM* jvm);
// This should be called from JNI_OnUnLoad.
void OnJniUnLoad(JavaVM* jvm);

}  // namespace webrtc

#endif  // SDK_ANDROID_NATIVE_API_SDK_JNI_H_
