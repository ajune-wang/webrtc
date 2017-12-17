/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/scoped_java_ref.h"

#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

#if RTC_DCHECK_IS_ON
// This constructor is inlined when DCHECKs are disabled; don't add anything
// else here.
JavaRef<jobject>::JavaRef(JNIEnv* env, jobject obj) : obj_(obj) {
  if (obj) {
    RTC_DCHECK(env && env->GetObjectRefType(obj) == JNILocalRefType);
  }
}
#endif

JNIEnv* JavaRef<jobject>::SetNewLocalRef(JNIEnv* env, jobject obj) {
  if (!env) {
    env = AttachCurrentThreadIfNeeded();
  } else {
    RTC_DCHECK_EQ(
        env, AttachCurrentThreadIfNeeded());  // Is |env| on correct thread.
  }
  if (obj)
    obj = env->NewLocalRef(obj);
  if (obj_)
    env->DeleteLocalRef(obj_);
  obj_ = obj;
  return env;
}

void JavaRef<jobject>::SetNewGlobalRef(JNIEnv* env, jobject obj) {
  if (!env) {
    env = AttachCurrentThreadIfNeeded();
  } else {
    RTC_DCHECK_EQ(
        env, AttachCurrentThreadIfNeeded());  // Is |env| on correct thread.
  }
  if (obj)
    obj = env->NewGlobalRef(obj);
  if (obj_)
    env->DeleteGlobalRef(obj_);
  obj_ = obj;
}

void JavaRef<jobject>::ResetLocalRef(JNIEnv* env) {
  if (obj_) {
    RTC_DCHECK_EQ(
        env, AttachCurrentThreadIfNeeded());  // Is |env| on correct thread.
    env->DeleteLocalRef(obj_);
    obj_ = nullptr;
  }
}

void JavaRef<jobject>::ResetGlobalRef() {
  if (obj_) {
    AttachCurrentThreadIfNeeded()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

jobject JavaRef<jobject>::ReleaseInternal() {
  jobject obj = obj_;
  obj_ = nullptr;
  return obj;
}

}  // namespace jni
}  // namespace webrtc
