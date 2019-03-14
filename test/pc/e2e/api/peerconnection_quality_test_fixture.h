/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PC_E2E_API_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_
#define TEST_PC_E2E_API_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "api/async_resolver_factory.h"
#include "api/call/call_factory_interface.h"
#include "api/fec_controller.h"
#include "api/media_transport_interface.h"
#include "api/peer_connection_interface.h"
#include "api/test/simulated_network.h"
#include "api/transport/network_control.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/api/audio_quality_analyzer_interface.h"
#include "test/pc/e2e/api/video_quality_analyzer_interface.h"

namespace webrtc {
namespace test {

struct InjectableComponents;
struct Params;

// TODO(titovartem) move to API when it will be stabilized.
class PeerConnectionE2EQualityTestFixture {
 public:
  // Contains screen share video stream properties.
  struct ScreenShareConfig {
    // If true, slides will be generated programmatically.
    bool generate_slides;
    // Shows how long one slide should be presented on the screen during
    // slide generation.
    TimeDelta slide_change_interval;
    // If equal to 0, no scrolling will be applied.
    TimeDelta scroll_duration;
    // If empty, default set of slides will be used.
    std::vector<std::string> slides_yuv_file_names;
  };

  enum VideoGeneratorType { kDefault, kI420A, kI010 };

  // Contains properties of single video stream.
  struct VideoConfig {
    VideoConfig(size_t width, size_t height, int32_t fps)
        : width(width), height(height), fps(fps) {}

    const size_t width;
    const size_t height;
    const int32_t fps;
    // Have to be unique among all specified configs for all peers in the call.
    // Will be auto generated if omitted.
    absl::optional<std::string> stream_label;
    // Only 1 from |generator|, |input_file_name| and |screen_share_config| can
    // be specified. If none of them are specified, then |generator| will be set
    // to VideoGeneratorType::kDefault.
    // If specified generator of this type will be used to produce input video.
    absl::optional<VideoGeneratorType> generator;
    // If specified this file will be used as input. Input video will be played
    // in a circle.
    absl::optional<std::string> input_file_name;
    // If specified screen share video stream will be created as input.
    absl::optional<ScreenShareConfig> screen_share_config;
    // Specifies spatial index of the video stream to analyze.
    // There are 3 cases:
    // 1. |target_spatial_index| omitted: in such case it will be assumed that
    //    video stream has not spatial layers and simulcast streams.
    // 2. |target_spatial_index| presented and simulcast encoder is used:
    //    in such case |target_spatial_index| will specify the index of
    //    simulcast stream, that should be analyzed. Other streams will be
    //    dropped.
    // 3. |target_spatial_index| presented and SVP encoder is used:
    //    in such case |target_spatial_index| will specify the top interesting
    //    spatial layer and all layers bellow, including target one will be
    //    processed. All layers above target one will be dropped.
    absl::optional<int> target_spatial_index;
    // If specified the input stream will be also copied to specified file.
    // It is actually one of the test's output file, which contains copy of what
    // was captured during the test for this video stream on sender side.
    // It is useful when generator is used as input.
    absl::optional<std::string> input_dump_file_name;
    // If specified this file will be used as output on the receiver side for
    // this stream. If multiple streams will be produced by input stream,
    // output files will be appended with indexes. The produced files contains
    // what was rendered for this video stream on receiver side.
    absl::optional<std::string> output_dump_file_name;
  };

  // Contains properties for audio in the call.
  struct AudioConfig {
    enum Mode {
      kGenerated,
      kFile,
    };
    // Have to be unique among all specified configs for all peers in the call.
    // Will be auto generated if omitted.
    absl::optional<std::string> stream_label;
    Mode mode = kGenerated;
    // Have to be specified only if mode = kFile
    absl::optional<std::string> input_file_name;
    // If specified the input stream will be also copied to specified file.
    absl::optional<std::string> input_dump_file_name;
    // If specified the output stream will be copied to specified file.
    absl::optional<std::string> output_dump_file_name;
    // Audio options to use.
    cricket::AudioOptions audio_options;
  };

  // PeerArgs is move only.
  class PeerArgs {
   public:
    PeerArgs() = default;
    PeerArgs(PeerArgs&) = delete;
    PeerArgs& operator=(PeerArgs&) = delete;
    PeerArgs(PeerArgs&&) = default;
    PeerArgs& operator=(PeerArgs&&) = default;
    virtual ~PeerArgs() = default;

    virtual PeerArgs* SetCallFactory(
        std::unique_ptr<CallFactoryInterface> call_factory) = 0;
    virtual PeerArgs* SetEventLogFactory(
        std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) = 0;
    virtual PeerArgs* SetFecControllerFactory(
        std::unique_ptr<FecControllerFactoryInterface>
            fec_controller_factory) = 0;
    virtual PeerArgs* SetNetworkControllerFactory(
        std::unique_ptr<NetworkControllerFactoryInterface>
            network_controller_factory) = 0;
    virtual PeerArgs* SetMediaTransportFactory(
        std::unique_ptr<MediaTransportFactory> media_transport_factory) = 0;
    virtual PeerArgs* SetVideoEncoderFactory(
        std::unique_ptr<VideoEncoderFactory> video_encoder_factory) = 0;
    virtual PeerArgs* SetVideoDecoderFactory(
        std::unique_ptr<VideoDecoderFactory> video_decoder_factory) = 0;

    virtual PeerArgs* SetAsyncResolverFactory(
        std::unique_ptr<webrtc::AsyncResolverFactory>
            async_resolver_factory) = 0;
    virtual PeerArgs* SetRTCCertificateGenerator(
        std::unique_ptr<rtc::RTCCertificateGeneratorInterface>
            cert_generator) = 0;
    virtual PeerArgs* SetSSLCertificateVerifier(
        std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier) = 0;

    virtual PeerArgs* AddVideoConfig(VideoConfig config) = 0;
    virtual PeerArgs* SetAudioConfig(AudioConfig config) = 0;
    virtual PeerArgs* SetRtcEventLogPath(std::string path) = 0;
    virtual PeerArgs* SetRTCConfiguration(
        PeerConnectionInterface::RTCConfiguration configuration) = 0;

    virtual std::unique_ptr<InjectableComponents> ReleaseComponents() = 0;
    virtual std::unique_ptr<Params> ReleaseParams() = 0;
  };

  // Contains parameters, that describe how long framework should run quality
  // test.
  struct RunParams {
    // Specifies how long the test should be run. This time shows how long
    // the media should flow after connection was established and before
    // it will be shut downed.
    TimeDelta run_duration;
  };

  virtual void Run(std::unique_ptr<PeerArgs> alice_args,
                   std::unique_ptr<PeerArgs> bob_args,
                   RunParams run_params) = 0;
  virtual ~PeerConnectionE2EQualityTestFixture() = default;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_API_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_
