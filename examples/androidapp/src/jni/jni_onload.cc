/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>
#undef JNIEXPORT
#define JNIEXPORT __attribute__((visibility("default")))

#include <android/log.h>
#define ALOGE(...) \
  __android_log_print(ANDROID_LOG_ERROR, "AppRTCMobile", __VA_ARGS__)

namespace AppRTCMobile {
namespace jni {

extern "C" jint JNIEXPORT JNICALL JNI_OnLoad(JavaVM* jvm, void* reserved) {
  ALOGE("JNI_OnLoad");
  JNIEnv* jni = nullptr;
  if (jvm->GetEnv(reinterpret_cast<void**>(&jni), JNI_VERSION_1_6) != JNI_OK)
    return -1;
  return JNI_VERSION_1_6;
}

}  // namespace jni
}  // namespace AppRTCMobile
