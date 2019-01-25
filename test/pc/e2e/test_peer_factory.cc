/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/test_peer_factory.h"

#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "logging/rtc_event_log/rtc_event_log_factory.h"
#include "media/engine/webrtc_media_engine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "p2p/client/basic_port_allocator.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/bind.h"
#include "rtc_base/location.h"
#include "rtc_base/network.h"
#include "test/frame_generator_capturer.h"
#include "test/pc/e2e/analyzer/video/default_encoded_image_id_injector.h"
#include "test/pc/e2e/analyzer/video/example_video_quality_analyzer.h"
#include "test/testsupport/copy_to_file_audio_capturer.h"

namespace webrtc {
namespace test {
namespace {

constexpr int16_t kGeneratedAudioMaxAmplitude = 32000;
constexpr int kSamplingFrequencyInHz = 48000;

}  // namespace

std::unique_ptr<TestPeer> TestPeerFactory::CreateTestPeer(
    std::unique_ptr<InjectableComponents> components,
    std::unique_ptr<Params> params,
    VideoQualityAnalyzerInjectionHelper* video_analyzer_holder,
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread,
    absl::optional<std::string> audio_output_file_name) {
  SetMandatoryEntities(components.get());
  params->rtc_configuration.sdp_semantics = SdpSemantics::kUnifiedPlan;

  std::unique_ptr<MockPeerConnectionObserver> observer =
      absl::make_unique<MockPeerConnectionObserver>();

  // Create peer connection factory.
  PeerConnectionFactoryDependencies pcf_deps = CreatePCFDependencies(
      components.get(), params.get(), video_analyzer_holder,
      components->network_thread, signaling_thread, worker_thread,
      std::move(audio_output_file_name));
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pcf =
      CreateModularPeerConnectionFactory(std::move(pcf_deps));

  // Create peer connection.
  PeerConnectionDependencies pc_deps =
      CreatePCDependencies(components.get(), observer.get());
  rtc::scoped_refptr<PeerConnectionInterface> pc =
      pcf->CreatePeerConnection(params->rtc_configuration, std::move(pc_deps));

  return absl::make_unique<TestPeer>(
      pcf, pc, std::move(observer), std::move(params),
      std::move(components->pc_dependencies->network_manager));
}

void TestPeerFactory::SetMandatoryEntities(InjectableComponents* components) {
  if (components->pcf_dependencies == nullptr) {
    components->pcf_dependencies =
        absl::make_unique<PeerConnectionFactoryComponents>();
  }
  if (components->pc_dependencies == nullptr) {
    components->pc_dependencies = absl::make_unique<PeerConnectionComponents>();
  }

  // Setup required peer connection factory dependencies.
  if (components->pcf_dependencies->call_factory == nullptr) {
    components->pcf_dependencies->call_factory = webrtc::CreateCallFactory();
  }
  if (components->pcf_dependencies->event_log_factory == nullptr) {
    components->pcf_dependencies->event_log_factory =
        webrtc::CreateRtcEventLogFactory();
  }
}

PeerConnectionFactoryDependencies TestPeerFactory::CreatePCFDependencies(
    InjectableComponents* components,
    Params* params,
    VideoQualityAnalyzerInjectionHelper* video_analyzer_holder,
    rtc::Thread* network_thread,
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread,
    absl::optional<std::string> audio_output_file_name) {
  PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.network_thread = network_thread;
  pcf_deps.signaling_thread = signaling_thread;
  pcf_deps.worker_thread = worker_thread;
  pcf_deps.media_engine =
      CreateMediaEngine(components, params, video_analyzer_holder,
                        std::move(audio_output_file_name));

  pcf_deps.call_factory = std::move(components->pcf_dependencies->call_factory);
  pcf_deps.event_log_factory =
      std::move(components->pcf_dependencies->event_log_factory);

  if (components->pcf_dependencies->fec_controller_factory != nullptr) {
    pcf_deps.fec_controller_factory =
        std::move(components->pcf_dependencies->fec_controller_factory);
  }
  if (components->pcf_dependencies->network_controller_factory != nullptr) {
    pcf_deps.network_controller_factory =
        std::move(components->pcf_dependencies->network_controller_factory);
  }
  if (components->pcf_dependencies->media_transport_factory != nullptr) {
    pcf_deps.media_transport_factory =
        std::move(components->pcf_dependencies->media_transport_factory);
  }

  return pcf_deps;
}

std::unique_ptr<cricket::MediaEngineInterface>
TestPeerFactory::CreateMediaEngine(
    InjectableComponents* components,
    Params* params,
    VideoQualityAnalyzerInjectionHelper* video_analyzer_holder,
    absl::optional<std::string> audio_output_file_name) {
  rtc::scoped_refptr<AudioDeviceModule> adm =
      CreateAudioDeviceModule(params, std::move(audio_output_file_name));

  std::unique_ptr<VideoEncoderFactory> video_encoder_factory =
      CreateVideoEncoderFactory(components, video_analyzer_holder);
  std::unique_ptr<VideoDecoderFactory> video_decoder_factory =
      CreateVideoDecoderFactory(components, video_analyzer_holder);

  return cricket::WebRtcMediaEngineFactory::Create(
      adm, webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      std::move(video_encoder_factory), std::move(video_decoder_factory),
      /*audio_mixer=*/nullptr, webrtc::AudioProcessingBuilder().Create());
}
rtc::scoped_refptr<AudioDeviceModule> TestPeerFactory::CreateAudioDeviceModule(
    Params* params,
    absl::optional<std::string> audio_output_file_name) {
  std::unique_ptr<TestAudioDeviceModule::Capturer> capturer;
  if (params->audio_config) {
    // If audio_config specified, create required audio capturer.
    if (params->audio_config->mode == AudioConfig::Mode::kGenerated) {
      capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(
          kGeneratedAudioMaxAmplitude, kSamplingFrequencyInHz);
    } else if (params->audio_config->mode == AudioConfig::Mode::kFile) {
      RTC_DCHECK(params->audio_config->input_file_name);
      capturer = TestAudioDeviceModule::CreateWavFileReader(
          params->audio_config->input_file_name.value());
    } else {
      RTC_NOTREACHED() << "Unknown params->audio_config->mode";
    }
  } else {
    // If we have no audio config we still need to provide some audio device.
    // In such case use generated capturer. Despite of we provided audio here,
    // in test media setup audio stream won't be added into peer connection.
    capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(
        kGeneratedAudioMaxAmplitude, kSamplingFrequencyInHz);
  }
  RTC_DCHECK(capturer);

  if (params->audio_config && params->audio_config->input_dump_file_name) {
    capturer = absl::make_unique<CopyToFileAudioCapturer>(
        std::move(capturer),
        params->audio_config->input_dump_file_name.value());
  }

  std::unique_ptr<TestAudioDeviceModule::Renderer> renderer;
  if (audio_output_file_name) {
    renderer = TestAudioDeviceModule::CreateBoundedWavFileWriter(
        audio_output_file_name.value(), kSamplingFrequencyInHz);
  } else {
    renderer =
        TestAudioDeviceModule::CreateDiscardRenderer(kSamplingFrequencyInHz);
  }

  return TestAudioDeviceModule::CreateTestAudioDeviceModule(
      std::move(capturer), std::move(renderer), /*speed=*/1.f);
}

std::unique_ptr<VideoEncoderFactory> TestPeerFactory::CreateVideoEncoderFactory(
    TestPeerFactory::InjectableComponents* components,
    VideoQualityAnalyzerInjectionHelper* video_analyzer_holder) {
  std::unique_ptr<VideoEncoderFactory> video_encoder_factory;
  if (components->pcf_dependencies->video_encoder_factory != nullptr) {
    video_encoder_factory =
        std::move(components->pcf_dependencies->video_encoder_factory);
  } else {
    video_encoder_factory = CreateBuiltinVideoEncoderFactory();
  }
  return video_analyzer_holder->WrapVideoEncoderFactory(
      std::move(video_encoder_factory));
}

std::unique_ptr<VideoDecoderFactory> TestPeerFactory::CreateVideoDecoderFactory(
    TestPeerFactory::InjectableComponents* components,
    VideoQualityAnalyzerInjectionHelper* video_analyzer_holder) {
  std::unique_ptr<VideoDecoderFactory> video_decoder_factory;
  if (components->pcf_dependencies->video_decoder_factory != nullptr) {
    video_decoder_factory =
        std::move(components->pcf_dependencies->video_decoder_factory);
  } else {
    video_decoder_factory = CreateBuiltinVideoDecoderFactory();
  }
  return video_analyzer_holder->WrapVideoDecoderFactory(
      std::move(video_decoder_factory));
}

PeerConnectionDependencies TestPeerFactory::CreatePCDependencies(
    InjectableComponents* components,
    PeerConnectionObserver* observer) {
  PeerConnectionDependencies pc_deps(observer);

  // We need to create network manager, because it is required for port
  // allocator. TestPeer will take ownership of this object and will store it
  // until the end of the test.
  if (components->pc_dependencies->network_manager == nullptr) {
    components->pc_dependencies->network_manager =
        // TODO(titovartem) have network manager integrated with emulated
        // network layer.
        absl::make_unique<rtc::BasicNetworkManager>();
  }
  auto port_allocator = absl::make_unique<cricket::BasicPortAllocator>(
      components->pc_dependencies->network_manager.get());

  // This test does not support TCP
  int flags = cricket::PORTALLOCATOR_DISABLE_TCP;
  port_allocator->set_flags(port_allocator->flags() | flags);

  pc_deps.allocator = std::move(port_allocator);

  if (components->pc_dependencies->async_resolver_factory != nullptr) {
    pc_deps.async_resolver_factory =
        std::move(components->pc_dependencies->async_resolver_factory);
  }
  if (components->pc_dependencies->cert_generator != nullptr) {
    pc_deps.cert_generator =
        std::move(components->pc_dependencies->cert_generator);
  }
  if (components->pc_dependencies->tls_cert_verifier != nullptr) {
    pc_deps.tls_cert_verifier =
        std::move(components->pc_dependencies->tls_cert_verifier);
  }
  return pc_deps;
}

}  // namespace test
}  // namespace webrtc
