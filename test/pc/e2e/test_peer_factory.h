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
  // because in such case it will be destroyed before peer connection and cause
  // SIGSEGV.
  std::unique_ptr<TestPeer> CreateTestPeer(
      std::unique_ptr<InjectableComponents> components,
      std::unique_ptr<Params> params,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_holder,
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
      InjectableComponents* components,
      Params* params,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_holder,
      rtc::Thread* network_thread,
      rtc::Thread* signaling_thread,
      rtc::Thread* worker_thread,
      absl::optional<std::string> audio_output_file_name);

  std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
      InjectableComponents* components,
      Params* params,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_holder,
      absl::optional<std::string> audio_output_file_name);

  rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
      Params* params,
      absl::optional<std::string> audio_output_file_name);

  std::unique_ptr<VideoEncoderFactory> CreateVideoEncoderFactory(
      InjectableComponents* components,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_holder);

  std::unique_ptr<VideoDecoderFactory> CreateVideoDecoderFactory(
      InjectableComponents* components,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_holder);

  // Creates PeerConnectionDependencies objects, providing entities
  // from InjectableComponents::PeerConnectionComponents.
  PeerConnectionDependencies CreatePCDependencies(
      InjectableComponents* components,
      PeerConnectionObserver* observer);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_TEST_PEER_FACTORY_H_
