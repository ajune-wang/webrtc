/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "media/base/codec.h"
#include "media/base/h264_profile_level_id.h"
#include "media/base/mediaconstants.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static bool IsSameH264Profile(const cricket::CodecParameterMap& params1,
                              const cricket::CodecParameterMap& params2) {
  const rtc::Optional<H264::ProfileLevelId> profile_level_id =
      H264::ParseSdpProfileLevelId(params1);
  const rtc::Optional<H264::ProfileLevelId> other_profile_level_id =
      H264::ParseSdpProfileLevelId(params2);
  // Compare H264 profiles, but not levels.
  return profile_level_id && other_profile_level_id &&
         profile_level_id->profile == other_profile_level_id->profile;
}

cricket::VideoCodec VideoCodecInfoToVideoCodec(JNIEnv* jni, jobject info) {
  jclass video_codec_info_class = jni->FindClass("org/webrtc/VideoCodecInfo");
  jfieldID name_field =
      GetFieldID(jni, video_codec_info_class, "name", "Ljava/lang/String;");
  jfieldID params_field =
      GetFieldID(jni, video_codec_info_class, "params", "Ljava/util/Map;");

  jobject j_params = GetObjectField(jni, info, params_field);
  jstring j_name = static_cast<jstring>(GetObjectField(jni, info, name_field));

  cricket::VideoCodec codec(JavaToStdString(jni, j_name));

  std::map<std::string, std::string> parameters =
      JavaToStdMapStrings(jni, j_params);
  for (auto const& param : parameters) {
    codec.SetParam(param.first, param.second);
  }

  return codec;
}

JNI_FUNCTION_DECLARATION(jboolean,
                         DefaultVideoEncoderFactory_isSameCodec,
                         JNIEnv* jni,
                         jclass,
                         jobject info1,
                         jobject info2) {
  cricket::VideoCodec codec1 = VideoCodecInfoToVideoCodec(jni, info1);
  cricket::VideoCodec codec2 = VideoCodecInfoToVideoCodec(jni, info2);

  if (!cricket::CodecNamesEq(codec1.name, codec2.name))
    return false;
  if (cricket::CodecNamesEq(codec1.name.c_str(), cricket::kH264CodecName) &&
      !IsSameH264Profile(codec1.params, codec2.params)) {
    return false;
  }
  return true;
}

}  // namespace jni
}  // namespace webrtc
