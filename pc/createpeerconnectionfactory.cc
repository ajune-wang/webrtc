/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/call/callfactoryinterface.h"
#include "api/peerconnectioninterface.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "media/engine/webrtcmediaengine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/bind.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"

namespace webrtc {

// The overloads in the #if block below use the WebRtcMediaEngineFactory
// constructor that takes "cricket::" video encoder/decoder factories as
// arguments, and implicitly uses built-in factories if they're null.

#if defined(USE_BUILTIN_SW_CODECS)
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory) {
  return CreatePeerConnectionFactory(
      nullptr /*network_thread*/, nullptr /*worker_thread*/,
      nullptr /*signaling_thread*/, nullptr /*default_adm*/,
      audio_encoder_factory, audio_decoder_factory,
      nullptr /*video_encoder_factory*/, nullptr /*video_decoder_factory*/,
      nullptr /*audio_mixer*/, nullptr /*audio_processing*/,
      nullptr /*fec_controller_factory*/,
      nullptr /*network_controller_factory*/);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreatePeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, nullptr /*audio_mixer*/,
      nullptr /*audio_processing*/, nullptr /*fec_controller_factory*/,
      nullptr /*network_controller_factory*/);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactoryWithAudioMixer(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer) {
  return CreatePeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer, nullptr /*audio_processing*/,
      nullptr /*fec_controller_factory*/,
      nullptr /*network_controller_factory*/);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing) {
  return CreatePeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer, audio_processing,
      nullptr /*fec_controller_factory*/,
      nullptr /*network_controller_factory*/);
}

// All CreatePeerConnectionFactory overloads in the USE_BUILTIN_SW_CODECS block
// should end up calling this.
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing,
    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory,
    std::unique_ptr<NetworkControllerFactoryInterface>
        network_controller_factory) {
  rtc::scoped_refptr<AudioProcessing> audio_processing_use = audio_processing;
  if (!audio_processing_use) {
    audio_processing_use = AudioProcessingBuilder().Create();
  }

  std::unique_ptr<cricket::MediaEngineInterface> media_engine(
      cricket::WebRtcMediaEngineFactory::Create(
          default_adm, audio_encoder_factory, audio_decoder_factory,
          video_encoder_factory, video_decoder_factory, audio_mixer,
          audio_processing_use));

  std::unique_ptr<CallFactoryInterface> call_factory = CreateCallFactory();

  std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory =
      CreateRtcEventLogFactory();

  return CreateModularPeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, std::move(media_engine),
      std::move(call_factory), std::move(event_log_factory),
      std::move(fec_controller_factory), std::move(network_controller_factory));
}

#endif

// The methods below use the new "VideoEncoderFactory"/"VideoDecoderFactory",
// and will NOT use built-in software codecs if they're null.

PeerConnectionFactoryDependencies PeerConnectionFactoryDependencies::Create() {
  return PeerConnectionFactoryDependencies();
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    rtc::scoped_refptr<AudioDeviceModule> default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing) {
  PeerConnectionFactoryDependencies deps =
      PeerConnectionFactoryDependencies::Create();
  deps.network_thread = network_thread;
  deps.worker_thread = worker_thread;
  deps.signaling_thread = signaling_thread;
  deps.audio_device_module = default_adm;
  deps.audio_encoder_factory = audio_encoder_factory;
  deps.audio_decoder_factory = audio_decoder_factory;
  deps.video_encoder_factory = std::move(video_encoder_factory);
  deps.video_decoder_factory = std::move(video_decoder_factory);
  deps.audio_mixer = audio_mixer;
  deps.audio_processing = audio_processing;
  return CreatePeerConnectionFactory(std::move(deps));
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies) {
  // TODO(deadbeef): Of the many "if null, default implementation will be
  // created" things, only AudioProcessing is created here. Would be nice to
  // create everything here (or in PeerConnectionFactoryDependencies::Create),
  // avoiding some interdependencies between build targets and simplifying
  // things.
  if (!dependencies.audio_processing)
    dependencies.audio_processing = AudioProcessingBuilder().Create();

  std::unique_ptr<cricket::MediaEngineInterface> media_engine =
      cricket::WebRtcMediaEngineFactory::Create(
          dependencies.audio_device_module, dependencies.audio_encoder_factory,
          dependencies.audio_decoder_factory,
          std::move(dependencies.video_encoder_factory),
          std::move(dependencies.video_decoder_factory),
          dependencies.audio_mixer, dependencies.audio_processing);

  std::unique_ptr<CallFactoryInterface> call_factory = CreateCallFactory();

  std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory =
      CreateRtcEventLogFactory();

  return CreateModularPeerConnectionFactory(
      dependencies.network_thread, dependencies.worker_thread,
      dependencies.signaling_thread, std::move(media_engine),
      std::move(call_factory), std::move(event_log_factory),
      std::move(dependencies.fec_controller_factory),
      std::move(dependencies.network_controller_factory));
}

}  // namespace webrtc
