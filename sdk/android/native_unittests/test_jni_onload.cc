/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
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

#include "rtc_base/checks.h"
#include "sdk/android/src/jni/class_reference_holder.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/jvm.h"

// This is called by the VM when the shared library is first loaded.
extern "C" jint JNIEXPORT JNICALL JNI_OnLoad(JavaVM* jvm, void* reserved) {
  jint ret = webrtc_jni::InitGlobalJniVariables(jvm);
  RTC_DCHECK_GE(ret, 0);
  if (ret < 0)
    return -1;

  webrtc_jni::LoadGlobalClassReferenceHolder();
  return ret;
}

extern "C" void JNIEXPORT JNICALL JNI_OnUnLoad(JavaVM* jvm, void* reserved) {
  webrtc_jni::FreeGlobalClassReferenceHolder();
}
