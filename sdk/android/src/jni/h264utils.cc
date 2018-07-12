/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videocodecinfo.h"

#include "common_video/h264/profile_level_id.h"
#include "sdk/android/generated_video_jni/jni/H264Utils_jni.h"

namespace webrtc {
namespace jni {

static jboolean JNI_VideoCodecInfo_IsSameH264Profile(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& params1,
    const JavaParamRef<jobject>& params2) {
  return H264::IsSameH264Profile(JavaToNativeStringMap(jni, params1),
                                 JavaToNativeStringMap(jni, params2));
}

}  // namespace jni
}  // namespace webrtc
