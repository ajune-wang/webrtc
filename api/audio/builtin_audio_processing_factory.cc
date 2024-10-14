/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/builtin_audio_processing_factory.h"

#include "modules/audio_processing/audio_processing_impl.h"
#include "rtc_base/logging.h"

namespace webrtc {

scoped_refptr<AudioProcessing> BuiltinAudioProcessingFactory::Create(
    const Environment& env) {
#ifdef WEBRTC_EXCLUDE_AUDIO_PROCESSING_MODULE
  // Users of CreatePeerConnectionFactory function should migrate to
  // CreateModularPeerConnectionFactory.
  //
  // Users of EnableMediaWithDefault should set media dependencies manualy
  // rather than rely on defaults and then should use EnableMedia.
  //
  // Users who inject BuiltinAudioProcessingFactory into
  // PeerConnectionFactoryDependencies just shouldn't inject it.

  // TODO: bugs.webrtc.org/369904700 - migrate run time log into build-time
  // warning, then remove ability to disable audio processing via built flag
  // altogether - it can be disabled at link time by not providing audio
  // processing factory.
  RTC_LOG(LS_ERROR)
      << "Please exclude dependency on `BuiltinAudioProcessingFactory` instead "
         "of disabling it via build flag".;
  // Return a null pointer when the APM is excluded from the build.
  return nullptr;
#else  // WEBRTC_EXCLUDE_AUDIO_PROCESSING_MODULE
  if (created_) {
    RTC_LOG(LS_WARNING) << "BuiltinAudioProcessingFactory is designed to be "
                           "used once. 2nd created AudioProcessing might "
                           "behave differentely than the 1st one.";
  }
  created_ = true;

  // TODO: bugs.webrtc.org/369904700 - Pass `env` or `env.field_trials()` when
  // AudioProcessingImpl gets constructor that accepts it.
  return make_ref_counted<AudioProcessingImpl>(
      env, config_, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
#endif
}

}  // namespace webrtc
