/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// Do not include this file directly. It's intended to be used only by the JNI
// generation script. We are exporting types in strange namespaces in order to
// be compatible with the generated code targeted for Chromium.

#ifndef SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
#define SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_

#include <jni.h>

#include <atomic>

#include "rtc_base/checks.h"
#include "third_party/jni_zero/jni_zero_internal.h"

// #define CHECK_CLAZZ(env, jcaller, clazz, ...) RTC_DCHECK(clazz);
// #d efine CHECK_NATIVE_PTR(env, jcaller, native_ptr, method_name, ...) \
//  RTC_DCHECK(native_ptr) << method_name;

#define BASE_EXPORT
#define JNI_REGISTRATION_EXPORT __attribute__((visibility("default")))

#if defined(WEBRTC_ARCH_X86)
// Dalvik JIT generated code doesn't guarantee 16-byte stack alignment on
// x86 - use force_align_arg_pointer to realign the stack at the JNI
// boundary. crbug.com/655248
#define JNI_GENERATOR_EXPORT \
  __attribute__((force_align_arg_pointer)) extern "C" JNIEXPORT JNICALL
#else
#define JNI_GENERATOR_EXPORT extern "C" JNIEXPORT JNICALL
#endif

#if defined(WEBRTC_ARCH_X86)
// Dalvik JIT generated code doesn't guarantee 16-byte stack alignment on
// x86 - use force_align_arg_pointer to realign the stack at the JNI
// boundary. crbug.com/655248
#define JNI_BOUNDARY_EXPORT \
  extern "C" __attribute__((visibility("default"), force_align_arg_pointer))
#else
#define JNI_BOUNDARY_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#if defined(COMPONENT_BUILD)
#define JNI_ZERO_COMPONENT_BUILD_EXPORT __attribute__((visibility("default")))
#else
#define JNI_ZERO_COMPONENT_BUILD_EXPORT
#endif

#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

namespace webrtc {
using namespace jni_zero;
}  // namespace webrtc

namespace jni_zero {
// Re-export relevant classes into the namespaces the script expects.

inline void CheckException(JNIEnv* env) {
  CHECK_EXCEPTION(env);
}

}  // namespace jni_zero

// Re-export helpers in the namespaces that the old jni_generator script
// expects.
// TODO(b/319078685): Remove once all uses of the jni_generator has been
// updated.
namespace base {
namespace android {
using jni_zero::JavaParamRef;
using jni_zero::JavaRef;
using jni_zero::MethodID;
using jni_zero::ScopedJavaLocalRef;
}  // namespace android
}  // namespace base
#endif  // SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
