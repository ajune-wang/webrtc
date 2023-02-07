/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "rtc_base/logging.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/test_native_utils_java/LoggableTestNativeUtils_jni.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(void,
                         LoggableTestNativeUtils_LogInfoTestMessage,
                         JNIEnv* jni,
                         jstring j_message) {
  std::string message =
      JavaToNativeString(jni, JavaParamRef<jstring>(j_message));
  RTC_LOG(LS_INFO) << message;
}

}  // namespace jni
}  // namespace webrtc
