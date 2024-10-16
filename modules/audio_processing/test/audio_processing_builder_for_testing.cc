/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"

#include <memory>
#include <utility>

#include "api/audio/builtin_audio_processing_factory.h"
#include "api/environment/environment_factory.h"
#include "modules/audio_processing/audio_processing_impl.h"

namespace webrtc {

AudioProcessingFactoryForTesting::AudioProcessingFactoryForTesting() = default;
AudioProcessingFactoryForTesting::~AudioProcessingFactoryForTesting() = default;

#ifdef WEBRTC_EXCLUDE_AUDIO_PROCESSING_MODULE

rtc::scoped_refptr<AudioProcessing> AudioProcessingFactoryForTesting::Create() {
  return rtc::make_ref_counted<AudioProcessingImpl>(
      config_, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
}

#else

scoped_refptr<AudioProcessing> AudioProcessingFactoryForTesting::Create(
    const Environment& env) {
  return BuiltinAudioProcessingFactory()
      .SetConfig(config_)
      .SetCapturePostProcessing(std::move(capture_post_processing_))
      .SetRenderPreProcessing(std::move(render_pre_processing_))
      .SetEchoControlFactory(std::move(echo_control_factory_))
      .SetEchoDetector(std::move(echo_detector_))
      .SetCaptureAnalyzer(std::move(capture_analyzer_))
      .Create(env);
}

#endif

scoped_refptr<AudioProcessing> AudioProcessingFactoryForTesting::Create() {
  return Create(CreateEnvironment());
}

}  // namespace webrtc
