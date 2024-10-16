/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>

#include "api/video_codecs/h264_profile_level_id.h"
#include "common_video/h264/sps_vui_rewriter.h"
#include "sdk/android/generated_video_jni/H264Utils_jni.h"
#include "sdk/android/src/jni/video_codec_info.h"

namespace webrtc {
namespace jni {

static jboolean JNI_H264Utils_IsSameH264Profile(
    JNIEnv* env,
    const JavaParamRef<jobject>& params1,
    const JavaParamRef<jobject>& params2) {
  return H264IsSameProfile(JavaToNativeStringMap(env, params1),
                           JavaToNativeStringMap(env, params2));
}

static jint JNI_H264Utils_RewriteVuiSps(JNIEnv* jni,
                                        const JavaParamRef<jobject>& spsBuffer,
                                        jint offset,
                                        jint len) {
  jint out_len = 0;
  void* ptr = jni->GetDirectBufferAddress(spsBuffer.obj());
  uint8_t* recv_data = static_cast<uint8_t*>(ptr) + offset;

  if (recv_data != nullptr) {
    int recv_data_size = jni->GetDirectBufferCapacity(spsBuffer.obj()) - offset;
    rtc::Buffer in_buffer;
    in_buffer.AppendData(recv_data, len);

    ColorSpace color_space;
    rtc::Buffer modified_buffer =
        SpsVuiRewriter::ParseOutgoingBitstreamAndRewrite(in_buffer,
                                                         &color_space);
    int buf_len = (int)modified_buffer.size();
    bool enough_space = buf_len <= recv_data_size;
    bool sps_found = (buf_len > 0);

    if (sps_found && enough_space) {
      std::memcpy(recv_data, modified_buffer.data(), buf_len);
      out_len = buf_len;
    }
  }

  return out_len;
}

}  // namespace jni
}  // namespace webrtc
