/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/logging.h"

#include <memory>

#include "rtc_base/generated_base_java_jni/Logging_jni.h"
#include "sdk/android/native_api/jni/java_types.h"

namespace webrtc {
namespace jni {

void JNI_Logging_EnableLogToDebugOutput(JNIEnv* jni, jint nativeSeverity) {
  if (nativeSeverity >= rtc::LS_VERBOSE && nativeSeverity <= rtc::LS_NONE) {
    rtc::LogMessage::LogToDebug(
        static_cast<rtc::LoggingSeverity>(nativeSeverity));
  }
}

void JNI_Logging_EnableLogThreads(JNIEnv* jni) {
  rtc::LogMessage::LogThreads(true);
}

void JNI_Logging_EnableLogTimeStamps(JNIEnv* jni) {
  rtc::LogMessage::LogTimestamps(true);
}

void JNI_Logging_Log(JNIEnv* jni,
                     jint j_severity,
                     jstring j_tag,
                     jstring j_message) {
  std::string message = JavaToStdString(jni, JavaParamRef<jstring>(j_message));
  std::string tag = JavaToStdString(jni, JavaParamRef<jstring>(j_tag));
  RTC_LOG_TAG(static_cast<rtc::LoggingSeverity>(j_severity), tag.c_str())
      << message;
}

}  // namespace jni
}  // namespace webrtc
