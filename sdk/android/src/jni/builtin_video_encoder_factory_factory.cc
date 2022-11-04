/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "sdk/android/generated_builtin_video_codec_factories_jni/BuiltinVideoEncoderFactoryFactory_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static jlong
JNI_BuiltinVideoEncoderFactoryFactory_CreateBuiltinVideoEncoderFactory(
    JNIEnv* env) {
  return NativeToJavaPointer(CreateBuiltinVideoEncoderFactory().release());
}

}  // namespace jni
}  // namespace webrtc
