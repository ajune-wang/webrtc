/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/voip/voip_engine_factory.h"

#include <memory>
#include <utility>

#include "api/environment/environment_factory.h"
#include "api/field_trials_view.h"
#include "api/voip/voip_engine.h"
#include "audio/voip/voip_core.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

std::unique_ptr<VoipEngine> CreateVoipEngine(VoipEngineConfig config) {
  RTC_CHECK(config.encoder_factory);
  RTC_CHECK(config.decoder_factory);
  RTC_CHECK(config.audio_device_module);

  Environment env = CreateEnvironment(std::move(config.field_trials),
                                      std::move(config.task_queue_factory));

  scoped_refptr<AudioProcessing> audio_processing =
      config.audio_processing_factory != nullptr
          ? config.audio_processing_factory->Create(env)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
          : std::move(config.audio_processing);
#pragma clang diagnostic pop

  if (audio_processing == nullptr) {
    RTC_DLOG(LS_INFO) << "No audio processing functionality provided.";
  }

  return std::make_unique<VoipCore>(
      env, std::move(config.encoder_factory), std::move(config.decoder_factory),
      std::move(config.audio_device_module), std::move(audio_processing));
}

}  // namespace webrtc
