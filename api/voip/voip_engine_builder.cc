/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/voip/voip_engine_builder.h"

#include <memory>
#include <utility>

#include "api/audio_codecs/audio_format.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "audio/voip/voip_core.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/logging.h"

namespace webrtc {

VoipEngineBuilder& VoipEngineBuilder::SetTaskQueueFactory(
    std::unique_ptr<TaskQueueFactory> task_queue_factory) {
  task_queue_factory_ = std::move(task_queue_factory);
  return *this;
}

VoipEngineBuilder& VoipEngineBuilder::SetAudioProcessing(
    std::unique_ptr<AudioProcessing> audio_processing) {
  audio_processing_ = std::move(audio_processing);
  return *this;
}

VoipEngineBuilder& VoipEngineBuilder::SetAudioDeviceModule(
    rtc::scoped_refptr<AudioDeviceModule> audio_device) {
  audio_device_ = audio_device;
  return *this;
}

VoipEngineBuilder& VoipEngineBuilder::SetAudioEncoderFactory(
    rtc::scoped_refptr<AudioEncoderFactory> encoder_factory) {
  encoder_factory_ = encoder_factory;
  return *this;
}

VoipEngineBuilder& VoipEngineBuilder::SetAudioDecoderFactory(
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory) {
  decoder_factory_ = decoder_factory;
  return *this;
}

std::unique_ptr<VoipEngine> VoipEngineBuilder::Create() {
  if (!encoder_factory_ || !decoder_factory_) {
    RTC_DLOG(LS_ERROR) << "Missing codec factory";
    return nullptr;
  }

  // Once we create voip engine, all compoments are unavailable.
  // Use local variables to make sure it doesn't have internal ones.
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory;

  encoder_factory.swap(encoder_factory_);
  decoder_factory.swap(decoder_factory_);

  if (!task_queue_factory_) {
    task_queue_factory_ = CreateDefaultTaskQueueFactory();
  }

  if (!audio_processing_) {
    RTC_DLOG(INFO) << "Creating default APM.";
    audio_processing_.reset(AudioProcessingBuilder().Create());
  }

  // If application set custom audio device module then use it.
  rtc::scoped_refptr<AudioDeviceModule> audio_device;
  if (audio_device_) {
    audio_device.swap(audio_device_);
  } else {
    RTC_DLOG(INFO) << "Creating default ADM.";
    audio_device = AudioDeviceModule::Create(
        AudioDeviceModule::kPlatformDefaultAudio, task_queue_factory_.get());
  }

  auto voip_core = std::make_unique<VoipCore>();

  if (!voip_core->Init(std::move(task_queue_factory_),
                       std::move(audio_processing_), audio_device,
                       encoder_factory, decoder_factory)) {
    RTC_DLOG(LS_ERROR) << "Failed to initialize voip core";
    voip_core.reset();
  }

  return voip_core;
}

void VoipEngineBuilder::SetLogLevel(absl::string_view log_level) {
  RTC_DCHECK(log_level.size() > 0);
  rtc::LogMessage::ConfigureLogging(log_level.data());
}

}  // namespace webrtc
