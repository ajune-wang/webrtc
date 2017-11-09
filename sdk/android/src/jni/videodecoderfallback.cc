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

#include "media/engine/videodecodersoftwarefallbackwrapper.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/videodecoderwrapper.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jlong,
                         VideoDecoderFallback_createNativeDecoder,
                         JNIEnv* jni,
                         jclass,
                         jobject j_fallback_decoder,
                         jobject j_primary_decoder) {
  jclass wrapped_native_decoder_class =
      FindClass(jni, "org/webrtc/WrappedNativeVideoDecoder");
  jmethodID get_native_decoder_method =
      jni->GetMethodID(wrapped_native_decoder_class, "getNativeDecoder", "()J");

  VideoDecoder* fallback_decoder;
  VideoDecoder* primary_decoder;

  if (jni->IsInstanceOf(j_fallback_decoder, wrapped_native_decoder_class)) {
    jlong native_decoder =
        jni->CallLongMethod(j_fallback_decoder, get_native_decoder_method);
    fallback_decoder = reinterpret_cast<VideoDecoder*>(native_decoder);
  } else {
    fallback_decoder = new VideoDecoderWrapper(jni, j_fallback_decoder);
  }

  if (jni->IsInstanceOf(j_primary_decoder, wrapped_native_decoder_class)) {
    jlong native_decoder =
        jni->CallLongMethod(j_primary_decoder, get_native_decoder_method);
    primary_decoder = reinterpret_cast<VideoDecoder*>(native_decoder);
  } else {
    primary_decoder = new VideoDecoderWrapper(jni, j_primary_decoder);
  }

  VideoDecoderSoftwareFallbackWrapper* nativeWrapper =
      new VideoDecoderSoftwareFallbackWrapper(
          std::unique_ptr<VideoDecoder>(fallback_decoder),
          std::unique_ptr<VideoDecoder>(primary_decoder));

  return jlongFromPointer(nativeWrapper);
}

}  // namespace jni
}  // namespace webrtc
