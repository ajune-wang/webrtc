/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "sdk/android/src/jni/pc/jni_logsink.h"

#include "sdk/android/generated_logging_jni/jni/JNILogging_jni.h"

namespace webrtc {
namespace jni {

JNILogSink::JNILogSink(JNIEnv* env, const JavaRef<jobject>& j_logging)
    : env_(env), j_logging_(env, j_logging) {}
JNILogSink::~JNILogSink() = default;

void JNILogSink::OnLogMessage(const std::string& msg,
                              rtc::LoggingSeverity severity,
                              const char* tag) {
  env_ = AttachCurrentThreadIfNeeded();
  Java_JNILogging_logToInjectable(
      env_, j_logging_, NativeToJavaString(env_, msg),
      NativeToJavaInteger(env_, severity), NativeToJavaString(env_, tag));
}

}  // namespace jni
}  // namespace webrtc
