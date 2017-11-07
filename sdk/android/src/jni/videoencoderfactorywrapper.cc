/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videoencoderfactorywrapper.h"

#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/logging.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/videoencoderwrapper.h"

namespace webrtc {
namespace jni {

VideoEncoderFactoryWrapper::VideoEncoderFactoryWrapper(JNIEnv* jni,
                                                       jobject encoder_factory)
    : video_codec_info_class_(jni, FindClass(jni, "org/webrtc/VideoCodecInfo")),
      hash_map_class_(jni, jni->FindClass("java/util/HashMap")),
      wrapped_native_encoder_class_(
          jni,
          FindClass(jni, "org/webrtc/WrappedNativeVideoEncoder")),
      encoder_factory_(jni, encoder_factory) {
  jclass encoder_factory_class = jni->GetObjectClass(*encoder_factory_);
  create_encoder_method_ = jni->GetMethodID(
      encoder_factory_class, "createEncoder",
      "(Lorg/webrtc/VideoCodecInfo;)Lorg/webrtc/VideoEncoder;");
  get_supported_codecs_method_ =
      jni->GetMethodID(encoder_factory_class, "getSupportedCodecs",
                       "()[Lorg/webrtc/VideoCodecInfo;");

  video_codec_info_constructor_ =
      jni->GetMethodID(*video_codec_info_class_, "<init>",
                       "(ILjava/lang/String;Ljava/util/Map;)V");
  name_field_ =
      jni->GetFieldID(*video_codec_info_class_, "name", "Ljava/lang/String;");
  params_field_ =
      jni->GetFieldID(*video_codec_info_class_, "params", "Ljava/util/Map;");

  hash_map_constructor_ = jni->GetMethodID(*hash_map_class_, "<init>", "()V");
  put_method_ = jni->GetMethodID(
      *hash_map_class_, "put",
      "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

  get_native_encoder_method_ = jni->GetMethodID(*wrapped_native_encoder_class_,
                                                "getNativeEncoder", "()J");

  supported_formats_ = GetSupportedFormats(jni);
}

std::unique_ptr<VideoEncoder> VideoEncoderFactoryWrapper::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject j_codec_info = ToJavaCodecInfo(jni, format);
  jobject encoder = jni->CallObjectMethod(*encoder_factory_,
                                          create_encoder_method_, j_codec_info);
  if (encoder != nullptr) {
    if (jni->IsInstanceOf(encoder, *wrapped_native_encoder_class_)) {
      jlong native_encoder =
          jni->CallLongMethod(encoder, get_native_encoder_method_);
      return std::unique_ptr<VideoEncoder>(
          reinterpret_cast<VideoEncoder*>(native_encoder));
    }
    return std::unique_ptr<VideoEncoder>(new VideoEncoderWrapper(jni, encoder));
  }
  return nullptr;
}

VideoEncoderFactory::CodecInfo VideoEncoderFactoryWrapper::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  jobject j_codec_info = ToJavaCodecInfo(jni, format);
  jobject encoder = jni->CallObjectMethod(*encoder_factory_,
                                          create_encoder_method_, j_codec_info);

  CodecInfo codec_info;
  // Check if this is a wrapped native software encoder implementation.
  codec_info.is_hardware_accelerated =
      !jni->IsInstanceOf(encoder, *wrapped_native_encoder_class_);
  codec_info.has_internal_source = false;
  return codec_info;
}

jobject VideoEncoderFactoryWrapper::ToJavaCodecInfo(
    JNIEnv* jni,
    const SdpVideoFormat& format) const {
  jobject j_params = jni->NewObject(*hash_map_class_, hash_map_constructor_);
  for (auto const& param : format.parameters) {
    jni->CallObjectMethod(j_params, put_method_,
                          JavaStringFromStdString(jni, param.first),
                          JavaStringFromStdString(jni, param.second));
  }
  return jni->NewObject(*video_codec_info_class_, video_codec_info_constructor_,
                        0 /* payload id */,
                        JavaStringFromStdString(jni, format.name), j_params);
}

std::vector<SdpVideoFormat> VideoEncoderFactoryWrapper::GetSupportedFormats(
    JNIEnv* jni) const {
  const jobjectArray j_supported_codecs = static_cast<jobjectArray>(
      jni->CallObjectMethod(*encoder_factory_, get_supported_codecs_method_));
  const jsize supported_codecs_count = jni->GetArrayLength(j_supported_codecs);

  std::vector<SdpVideoFormat> supported_formats;
  for (jsize i = 0; i < supported_codecs_count; i++) {
    jobject j_supported_codec =
        jni->GetObjectArrayElement(j_supported_codecs, i);
    jobject j_params = jni->GetObjectField(j_supported_codec, params_field_);
    jstring j_name = static_cast<jstring>(
        jni->GetObjectField(j_supported_codec, name_field_));
    supported_formats.push_back(SdpVideoFormat(
        JavaToStdString(jni, j_name), JavaToStdMapStrings(jni, j_params)));
  }
  return supported_formats;
}

}  // namespace jni
}  // namespace webrtc
