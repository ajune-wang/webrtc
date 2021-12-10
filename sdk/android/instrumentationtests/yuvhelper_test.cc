/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(void,
                         YuvHelperTest_nativeMemsetZero,
                         JNIEnv* jni,
                         jclass,
                         jobject j_bytebuffer) {
  void* buffer =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_bytebuffer));
  CHECK_EXCEPTION(jni) << "error GetDirectBufferAddress";
  size_t size = static_cast<size_t>(jni->GetDirectBufferCapacity(j_bytebuffer));
  CHECK_EXCEPTION(jni) << "error GetDirectBufferCapacity";
  memset(buffer, 0, size);
}

}  // namespace jni
}  // namespace webrtc
