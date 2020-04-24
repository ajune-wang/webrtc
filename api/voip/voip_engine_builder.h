/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_ENGINE_BUILDER_H_
#define API_VOIP_VOIP_ENGINE_BUILDER_H_

#include <memory>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/voip/voip_engine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class VoipEngineBuilder {
 public:
  VoipEngineBuilder() = default;

  // Set log level of the VoipEngine as defined per
  // LogMessage::ConfigureLogging logic (rtc_base/logging.cc)
  static void SetLogLevel(absl::string_view log_level);

  // VoipEngineBuilder takes the ownership of the components set here.
  // Except AudioEncoderFactory/AudioDecoderFactory, all other components
  // are optional and default will be created when not set by application.
  VoipEngineBuilder& SetTaskQueueFactory(
      std::unique_ptr<TaskQueueFactory> task_queue_factory);
  VoipEngineBuilder& SetAudioProcessing(
      std::unique_ptr<AudioProcessing> audio_processing);
  VoipEngineBuilder& SetAudioDeviceModule(
      rtc::scoped_refptr<AudioDeviceModule> audio_device);
  VoipEngineBuilder& SetAudioEncoderFactory(
      rtc::scoped_refptr<AudioEncoderFactory> encoder_factory);
  VoipEngineBuilder& SetAudioDecoderFactory(
      rtc::scoped_refptr<AudioDecoderFactory> decoder_factory);

  // Create a VoipEngine instance using compoent set via above methods.
  // Components are moved that they won't be available with next Create().
  std::unique_ptr<VoipEngine> Create();

 private:
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  std::unique_ptr<AudioProcessing> audio_processing_;
  rtc::scoped_refptr<AudioDeviceModule> audio_device_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_ENGINE_BUILDER_H_
