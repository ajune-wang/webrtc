/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/api/create_peerconnection_quality_test_fixture.h"

#include <utility>

#include "absl/memory/memory.h"
#include "test/pc/e2e/peer_connection_quality_test.h"
#include "test/pc/e2e/peer_connection_quality_test_params.h"

namespace webrtc {
namespace test {
namespace {

class PeerArgsImpl : public PeerConnectionE2EQualityTestFixture::PeerArgs {
 public:
  PeerArgsImpl(rtc::Thread* network_thread,
               rtc::NetworkManager* network_manager)
      : components_(absl::make_unique<InjectableComponents>(network_thread,
                                                            network_manager)),
        params_(absl::make_unique<Params>()) {}

  PeerArgs* SetCallFactory(
      std::unique_ptr<CallFactoryInterface> call_factory) override {
    components_->pcf_dependencies->call_factory = std::move(call_factory);
    return this;
  }
  PeerArgs* SetEventLogFactory(
      std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) override {
    components_->pcf_dependencies->event_log_factory =
        std::move(event_log_factory);
    return this;
  }
  PeerArgs* SetFecControllerFactory(
      std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory)
      override {
    components_->pcf_dependencies->fec_controller_factory =
        std::move(fec_controller_factory);
    return this;
  }
  PeerArgs* SetNetworkControllerFactory(
      std::unique_ptr<NetworkControllerFactoryInterface>
          network_controller_factory) override {
    components_->pcf_dependencies->network_controller_factory =
        std::move(network_controller_factory);
    return this;
  }
  PeerArgs* SetMediaTransportFactory(
      std::unique_ptr<MediaTransportFactory> media_transport_factory) override {
    components_->pcf_dependencies->media_transport_factory =
        std::move(media_transport_factory);
    return this;
  }
  PeerArgs* SetVideoEncoderFactory(
      std::unique_ptr<VideoEncoderFactory> video_encoder_factory) override {
    components_->pcf_dependencies->video_encoder_factory =
        std::move(video_encoder_factory);
    return this;
  }
  PeerArgs* SetVideoDecoderFactory(
      std::unique_ptr<VideoDecoderFactory> video_decoder_factory) override {
    components_->pcf_dependencies->video_decoder_factory =
        std::move(video_decoder_factory);
    return this;
  }

  PeerArgs* SetAsyncResolverFactory(
      std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory)
      override {
    components_->pc_dependencies->async_resolver_factory =
        std::move(async_resolver_factory);
    return this;
  }
  PeerArgs* SetRTCCertificateGenerator(
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator)
      override {
    components_->pc_dependencies->cert_generator = std::move(cert_generator);
    return this;
  }
  PeerArgs* SetSSLCertificateVerifier(
      std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier) override {
    components_->pc_dependencies->tls_cert_verifier =
        std::move(tls_cert_verifier);
    return this;
  }

  PeerArgs* AddVideoConfig(
      PeerConnectionE2EQualityTestFixture::VideoConfig config) override {
    params_->video_configs.push_back(std::move(config));
    return this;
  }
  PeerArgs* SetAudioConfig(
      PeerConnectionE2EQualityTestFixture::AudioConfig config) override {
    params_->audio_config = std::move(config);
    return this;
  }
  PeerArgs* SetRtcEventLogPath(std::string path) override {
    params_->rtc_event_log_path = std::move(path);
    return this;
  }
  PeerArgs* SetRTCConfiguration(
      PeerConnectionInterface::RTCConfiguration configuration) override {
    params_->rtc_configuration = std::move(configuration);
    return this;
  }

  std::unique_ptr<InjectableComponents> ReleaseComponents() override {
    return std::move(components_);
  }

  std::unique_ptr<Params> ReleaseParams() override {
    return std::move(params_);
  }

 private:
  std::unique_ptr<InjectableComponents> components_;
  std::unique_ptr<Params> params_;
};

}  // namespace

std::unique_ptr<PeerConnectionE2EQualityTestFixture>
CreatePeerConnectionE2EQualityTestFixture(
    std::string test_case_name,
    std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer,
    std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer) {
  return absl::make_unique<webrtc::test::PeerConnectionE2EQualityTest>(
      std::move(test_case_name), std::move(audio_quality_analyzer),
      std::move(video_quality_analyzer));
}

std::unique_ptr<PeerConnectionE2EQualityTestFixture::PeerArgs>
CreatePeerConnectionE2EQualityTestFixturePeerArgs(
    rtc::Thread* network_thread,
    rtc::NetworkManager* network_manager) {
  return absl::make_unique<PeerArgsImpl>(network_thread, network_manager);
}

}  // namespace test
}  // namespace webrtc
