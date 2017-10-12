/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/audio_jni.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

rtc::scoped_refptr<AudioDecoderFactory> CreateAudioDecoderFactory() {
  return CreateBuiltinAudioDecoderFactory();
}

rtc::scoped_refptr<AudioEncoderFactory> CreateAudioEncoderFactory() {
  return CreateBuiltinAudioEncoderFactory();
}

rtc::scoped_refptr<AudioProcessing> CreateAudioProcessing() {
  return AudioProcessing::Create();
}

JNI_FUNCTION_DECLARATION(jlong,
                         AudioProcessing_nativeCreateAudioProcessing,
                         JNIEnv*,
                         jclass,
                         jlong jpost_processor) {
  std::unique_ptr<PostProcessing> post_processor(
      reinterpret_cast<PostProcessing*>(jpost_processor));
  rtc::scoped_refptr<AudioProcessing> audio_processing =
      AudioProcessing::Create(webrtc::Config(), std::move(post_processor),
                              nullptr /* beamformer */);
  return jlongFromPointer(audio_processing.release());
}

}  // namespace jni
}  // namespace webrtc
