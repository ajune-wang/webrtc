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

#include <utility>

#include "audio/voip/voip_core.h"
#include "rtc_base/logging.h"

namespace webrtc {

std::unique_ptr<VoipEngine> CreateVoipEngine(VoipEngineConfig config) {
  if (!config.encoder_factory || !config.decoder_factory) {
    RTC_DLOG(LS_ERROR) << "Missing codec factory";
    return nullptr;
  }

  if (!config.task_queue_factory) {
    RTC_DLOG(LS_ERROR) << "Missing task queue factory";
    return nullptr;
  }

  std::unique_ptr<AudioProcessing> audio_processing;
  if (config.audio_processing) {
    audio_processing = std::move(config.audio_processing.value());
    RTC_DLOG(INFO) << "Using " << (!audio_processing ? "no" : "custom")
                   << " AudioProcessing";
  } else {
    RTC_DLOG(INFO) << "Using default AudioProcessing.";
    audio_processing.reset(AudioProcessingBuilder().Create());
  }

  // If application set custom audio device module then use it.
  rtc::scoped_refptr<AudioDeviceModule> audio_device;
  if (config.audio_device && config.audio_device.value()) {
    RTC_DLOG(INFO) << "Using custom audio device";
    audio_device.swap(config.audio_device.value());
  } else {
    RTC_DLOG(INFO) << "Using default audio device";
    audio_device =
        AudioDeviceModule::Create(AudioDeviceModule::kPlatformDefaultAudio,
                                  config.task_queue_factory.get());
  }

  auto voip_core = std::make_unique<VoipCore>();

  if (!voip_core->Init(std::move(config.task_queue_factory),
                       std::move(audio_processing), audio_device,
                       config.encoder_factory, config.decoder_factory)) {
    RTC_DLOG(LS_ERROR) << "Failed to initialize voip core";
    voip_core.reset();
  }

  return voip_core;
}

}  // namespace webrtc
