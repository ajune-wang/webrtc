/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_ENGINE_FACTORY_H_
#define API_VOIP_VOIP_ENGINE_FACTORY_H_

#include <memory>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/voip/voip_engine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

// VoipEngineConfig is a struct that defines required parameters to
// instantiate a VoipEngine instance through CreateVoipEngine factory method.
// Each member is marked with comments as either mandatory or optional and
// default implementations that applications can use.
struct VoipEngineConfig {
  // Mandatory (e.g. api/audio_codec/builtin_audio_encoder_factory).
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory;

  // Mandatory (e.g. api/audio_codec/builtin_audio_decoder_factory).
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory;

  // Mandatory (e.g. api/task_queue/default_task_queue_factory).
  std::unique_ptr<TaskQueueFactory> task_queue_factory;

  // Mandatory (e.g. modules/audio_device/include).
  rtc::scoped_refptr<AudioDeviceModule> audio_device_module;

  // Optional (e.g. modules/audio_processing/include).
  // When this is not set, VoipEngine will skip the audio processing such as
  // acoustic echo cancellation, noise suppression, gain control, and etc.
  rtc::scoped_refptr<AudioProcessing> audio_processing;
};

// This could return nullptr if AudioDeviceModule (ADM) initialization fails
// during construction of VoipEngine. VoipEngine initialization includes
// checking default microphone/speaker devices configured in the underlying
// system but the failures to check these won't return nullptr here as there
// could be multiple audio devices configured and other devices may work fine.
std::unique_ptr<VoipEngine> CreateVoipEngine(VoipEngineConfig config);

}  // namespace webrtc

#endif  // API_VOIP_VOIP_ENGINE_FACTORY_H_
