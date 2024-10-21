/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/audio.h"

#include "api/audio/audio_processing.h"
#include "api/audio/builtin_audio_processing_factory.h"
#include "api/environment/environment_factory."

namespace webrtc {
namespace jni {

rtc::scoped_refptr<AudioProcessing> CreateAudioProcessing() {
  return BuiltinAudioProcessingFactory().Create(CreateEnvironment());
}

}  // namespace jni
}  // namespace webrtc
