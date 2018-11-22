/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_NATIVECAPTUREROBSERVER_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_NATIVECAPTUREROBSERVER_H_

#include <jni.h>

#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/androidvideotracksource.h"

namespace webrtc {
namespace jni {

ScopedJavaLocalRef<jobject> CreateJavaNativeCapturerObserver(
    JNIEnv* env,
    rtc::scoped_refptr<AndroidVideoTrackSource> native_source);

}  // namespace jni
}  // namespace webrtc

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_NATIVECAPTUREROBSERVER_H_
