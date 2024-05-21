/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_parameters.h"
#include "api/video_codecs/h264_profile_level_id.h"
#include "sdk/android/generated_video_jni/H264Utils_jni.h"
#include "sdk/android/src/jni/video_codec_info.h"

namespace webrtc {
namespace jni {

static jboolean JNI_H264Utils_IsSameH264Profile(
    JNIEnv* env,
    const JavaParamRef<jobject>& params1,
    const JavaParamRef<jobject>& params2) {
  return H264IsSameProfile(
      JavaToStringToStringContainer<CodecParameterMap>(env, params1),
      JavaToStringToStringContainer<CodecParameterMap>(env, params2));
}

}  // namespace jni
}  // namespace webrtc
