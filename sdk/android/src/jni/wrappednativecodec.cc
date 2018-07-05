/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/wrappednativecodec.h"

#include "sdk/android/generated_video_jni/jni/WrappedNativeVideoEncoder_jni.h"

namespace webrtc {
namespace jni {

bool IsWrappedSoftwareEncoder(JNIEnv* jni, const JavaRef<jobject>& j_encoder) {
  return Java_WrappedNativeVideoEncoder_isWrappedSoftwareEncoder(jni,
                                                                 j_encoder);
}

}  // namespace jni
}  // namespace webrtc
