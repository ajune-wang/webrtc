/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_TEST_PEER_FACTORY_H_
#define TEST_PC_E2E_TEST_PEER_FACTORY_H_

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "media/base/media_engine.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/analyzer/video/encoded_image_id_injector.h"
#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"
#include "test/pc/e2e/api/peerconnection_quality_test_fixture.h"
#include "test/pc/e2e/test_peer.h"

namespace webrtc {
namespace test {

// Factory to create call's peers. It will setup all components, that should be
// provided to WebRTC PeerConnectionFactory and PeerConnection creation methods,
// also will setup dependencies, that are required for media analyzers
// injection.
//
// Client should first create factory: TestPeerFactory factory();
// And then client can use it to create peers like this:
//   1. With all defaults provided by framework:
//      factory.CreateTestPeer(
//          absl::make_unique<InjectableComponents>(network_thread),
//          absl::make_unique<Params>(),
//          video_analyzer_helper,
//          signaling_thread,
//          working_thread,
//          audio_output_file_name);
//   2. Or client can create and modify InjectableComponents and Params and then
//      pass them.
class TestPeerFactory {
 public:
  using Params = PeerConnectionE2EQualityTestFixture::Params;
  using InjectableComponents =
      PeerConnectionE2EQualityTestFixture::InjectableComponents;
  using PeerConnectionFactoryComponents =
      PeerConnectionE2EQualityTestFixture::PeerConnectionFactoryComponents;
  using PeerConnectionComponents =
      PeerConnectionE2EQualityTestFixture::PeerConnectionComponents;
  using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;

  TestPeerFactory() = default;

  // We require |worker_thread| here, because TestPeer can't own worker thread,
  // because in such case it will be destroyed before peer connection.
  // |signaling_thread| will be provided by test fixture implementation.
  // |params| - describes current peer paramters, like current peer video
  // streams and audio streams
  // |audio_outpu_file_name| - the name of output file, where incoming audio
  // stream should be written. It should be provided from remote peer
  // |params.audio_config.output_file_name|
  std::unique_ptr<TestPeer> CreateTestPeer(
      std::unique_ptr<InjectableComponents> components,
      std::unique_ptr<Params> params,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      rtc::Thread* signaling_thread,
      rtc::Thread* worker_thread,
      absl::optional<std::string> audio_output_file_name);

 private:
  // Sets mandatory entities in injectable components like |pcf_dependencies|
  // and |pc_dependencies| if they are omitted. Also setup required
  // dependencies, that won't be specially provided by factory and will be just
  // transferred to peer connection creation code.
  void SetMandatoryEntities(InjectableComponents* components);

  // Creates PeerConnectionFactoryDependencies objects, providing entities
  // from InjectableComponents::PeerConnectionFactoryComponents and also
  // creating entities, that are required for correct injection of media quality
  // analyzers.
  PeerConnectionFactoryDependencies CreatePCFDependencies(
      std::unique_ptr<PeerConnectionFactoryComponents> pcf_dependencies,
      absl::optional<AudioConfig> audio_config,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      rtc::Thread* network_thread,
      rtc::Thread* signaling_thread,
      rtc::Thread* worker_thread,
      absl::optional<std::string> audio_output_file_name);

  std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
      PeerConnectionFactoryComponents* pcf_dependencies,
      absl::optional<AudioConfig> audio_config,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      absl::optional<std::string> audio_output_file_name);

  rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
      absl::optional<AudioConfig> audio_config,
      absl::optional<std::string> audio_output_file_name);

  std::unique_ptr<TestAudioDeviceModule::Capturer> CreateAudioCapturer(
      AudioConfig audio_config);

  std::unique_ptr<VideoEncoderFactory> CreateVideoEncoderFactory(
      PeerConnectionFactoryComponents* pcf_dependencies,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper);

  std::unique_ptr<VideoDecoderFactory> CreateVideoDecoderFactory(
      PeerConnectionFactoryComponents* pcf_dependencies,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper);

  // Creates PeerConnectionDependencies objects, providing entities
  // from InjectableComponents::PeerConnectionComponents.
  PeerConnectionDependencies CreatePCDependencies(
      PeerConnectionComponents* pc_dependencies,
      PeerConnectionObserver* observer);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_TEST_PEER_FACTORY_H_
