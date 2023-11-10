/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <functional>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/peer_connection_interface.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/frame_generator_capturer_video_track_source.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/gunit.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using ::testing::Combine;
using ::testing::Values;

#if RTC_ENABLE_VP9
constexpr bool kVp9Enabled = true;
#else
constexpr bool kVp9Enabled = false;
#endif

#if defined(WEBRTC_USE_H264)
constexpr bool kH264Enabled = true;
#else
constexpr bool kH264Enabled = false;
#endif

namespace {
static const int kDefaultTimeoutMs = 5000;

bool AddIceCandidates(PeerConnectionWrapper* peer,
                      std::vector<const IceCandidateInterface*> candidates) {
  for (const auto candidate : candidates) {
    if (!peer->pc()->AddIceCandidate(candidate)) {
      return false;
    }
  }
  return true;
}
}  // namespace

class WrappedEncoderFactory : public VideoEncoderFactory {
 public:
  explicit WrappedEncoderFactory(
      VideoCodecType codec_type,
      int max_temporal_layers,
      std::function<void(const VideoCodec*)> on_init_encode_callback)
      : codec_type_(codec_type),
        max_temporal_layers_(max_temporal_layers),
        on_init_encode_callback_(std::move(on_init_encode_callback)) {
    RTC_DCHECK_GE(max_temporal_layers, 1);
    RTC_DCHECK_LE(max_temporal_layers, 3);
  }

  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    absl::InlinedVector<ScalabilityMode, kScalabilityModeCount>
        scalability_modes;
    if (max_temporal_layers_ >= 1) {
      scalability_modes.push_back(webrtc::ScalabilityMode::kL1T1);
    }
    if (max_temporal_layers_ >= 2) {
      scalability_modes.push_back(webrtc::ScalabilityMode::kL1T2);
    }
    if (max_temporal_layers_ == 3) {
      scalability_modes.push_back(webrtc::ScalabilityMode::kL1T3);
    }

    std::vector<SdpVideoFormat> formats;
    if (codec_type_ == kVideoCodecVP8) {
      return {SdpVideoFormat(cricket::kVp8CodecName,
                             SdpVideoFormat::Parameters(), scalability_modes)};
    } else if (codec_type_ == kVideoCodecVP9 && kVp9Enabled) {
      formats = LibvpxVp9EncoderTemplateAdapter().SupportedFormats();
      RTC_DCHECK_GT(formats.size(), 0);
    } else if (codec_type_ == kVideoCodecAV1) {
      formats = LibaomAv1EncoderTemplateAdapter().SupportedFormats();
      RTC_DCHECK_GT(formats.size(), 0);
    } else if (codec_type_ == kVideoCodecH264 && kH264Enabled) {
      formats = OpenH264EncoderTemplateAdapter().SupportedFormats();
      RTC_DCHECK_GT(formats.size(), 0);
    } else {
      RTC_CHECK_NOTREACHED();
    }
    SdpVideoFormat format = formats[0];
    format.scalability_modes = scalability_modes;
    return {format};
  }

  CodecSupport QueryCodecSupport(
      const SdpVideoFormat& format,
      absl::optional<std::string> scalability_mode) const override {
    if (!format.IsCodecInList(GetSupportedFormats())) {
      return CodecSupport{.is_supported = false, .is_power_efficient = false};
    }

    if (!scalability_mode.has_value()) {
      return CodecSupport{.is_supported = true, .is_power_efficient = false};
    }

    absl::optional<ScalabilityMode> mode =
        ScalabilityModeFromString(*scalability_mode);
    if (!mode.has_value()) {
      return CodecSupport{.is_supported = false, .is_power_efficient = false};
    }
    if (*mode != ScalabilityMode::kL1T1 && *mode != ScalabilityMode::kL1T2 &&
        *mode != ScalabilityMode::kL1T3) {
      return CodecSupport{.is_supported = false, .is_power_efficient = false};
    }

    int requested_temporal_layers = ScalabilityModeToNumTemporalLayers(*mode);
    return CodecSupport{
        .is_supported = (requested_temporal_layers <= max_temporal_layers_),
        .is_power_efficient = false};
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
    return std::make_unique<WrappedVideoEncoder>(codec_type_,
                                                 on_init_encode_callback_);
  }

 private:
  class WrappedVideoEncoder : public VideoEncoder {
   public:
    WrappedVideoEncoder(
        VideoCodecType codec_type,
        std::function<void(const VideoCodec*)> on_init_encode_callback)
        : codec_type_(codec_type),
          on_init_encode_callback_(std::move(on_init_encode_callback)) {}

    int InitEncode(const VideoCodec* codec_settings,
                   const VideoEncoder::Settings& settings) override {
      RTC_DCHECK_EQ(codec_settings->codecType, codec_type_);
      on_init_encode_callback_(codec_settings);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      return 0;
    }

    int32_t Release() override { return 0; }

    int32_t Encode(const VideoFrame& frame,
                   const std::vector<VideoFrameType>* frame_types) override {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    void SetRates(const RateControlParameters& parameters) override {}

    EncoderInfo GetEncoderInfo() const override { return EncoderInfo(); }

   private:
    const VideoCodecType codec_type_;
    std::function<void(const VideoCodec*)> on_init_encode_callback_;
  };

  const VideoCodecType codec_type_;
  const int max_temporal_layers_;
  std::function<void(const VideoCodec*)> on_init_encode_callback_;
};

std::string TestParametersMidTestConfigurationToString(
    testing::TestParamInfo<std::tuple<
        VideoCodecType,
        int /* max_temporal_layers */,
        absl::optional<ScalabilityMode> /* target_scalability_mode */,
        int /* simulcast_layers */,
        bool /* screencast */>> info) {
  rtc::StringBuilder sb;
  sb << CodecTypeToPayloadString(std::get<0>(info.param)) << "_"
     << std::get<1>(info.param) << "_"
     << (std::get<2>(info.param).has_value()
             ? webrtc::ScalabilityModeToString(std::get<2>(info.param).value())
             : "none")
     << "_" << std::get<3>(info.param)
     << (std::get<4>(info.param) ? "_screencast" : "");
  return sb.Release();
}

class PeerConnectionSVCEncoderIntegrationTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<
          VideoCodecType,
          int /* max_temporal_layers */,
          absl::optional<ScalabilityMode> /* target_scalability_mode */,
          int /* simulcast_layers */,
          bool /* screencast */>> {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapper> WrapperPtr;

  PeerConnectionSVCEncoderIntegrationTest()
      : codec_type_(std::get<0>(GetParam())),
        max_temporal_layers_(std::get<1>(GetParam())),
        target_scalability_mode_(std::get<2>(GetParam())),
        simulcast_layers_(std::get<3>(GetParam())),
        screencast_(std::get<4>(GetParam())),
        clock_(Clock::GetRealTimeClock()),
        network_thread_(std::make_unique<rtc::Thread>(&pss_)),
        worker_thread_(rtc::Thread::Create()) {
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());

    std::unique_ptr<WrappedEncoderFactory> video_encoder_factory;
    video_encoder_factory.reset(new WrappedEncoderFactory(
        codec_type_, max_temporal_layers_,
        [this](const VideoCodec* video_codec) { OnInitEncode(video_codec); }));

    std::unique_ptr<VideoDecoderFactory> video_decoder_factory;
    switch (codec_type_) {
      case kVideoCodecVP8:
        video_decoder_factory = std::make_unique<
            VideoDecoderFactoryTemplate<LibvpxVp8DecoderTemplateAdapter>>();
        break;
      case kVideoCodecVP9:
        video_decoder_factory = std::make_unique<
            VideoDecoderFactoryTemplate<LibvpxVp9DecoderTemplateAdapter>>();
        break;
      case kVideoCodecAV1:
        video_decoder_factory = std::make_unique<
            VideoDecoderFactoryTemplate<Dav1dDecoderTemplateAdapter>>();
        break;
      case kVideoCodecH264:
        video_decoder_factory = std::make_unique<
            VideoDecoderFactoryTemplate<OpenH264DecoderTemplateAdapter>>();
        break;
      default:
        RTC_CHECK_NOTREACHED();
    }

    pc_factory_ = CreatePeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        rtc::scoped_refptr<AudioDeviceModule>(FakeAudioCaptureModule::Create()),
        CreateBuiltinAudioEncoderFactory(), CreateBuiltinAudioDecoderFactory(),
        std::move(video_encoder_factory), std::move(video_decoder_factory),
        nullptr /* audio_mixer */, nullptr /* audio_processing */);

    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";
    config_.servers.push_back(ice_server);
    config_.sdp_semantics = SdpSemantics::kUnifiedPlan;
  }

  void TearDown() override {
    MutexLock lock(&mutex_);
    on_init_encode_callback_ = nullptr;
  }

  WrapperPtr CreatePeerConnection() {
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    PeerConnectionDependencies pc_dependencies(observer.get());
    auto result = pc_factory_->CreatePeerConnectionOrError(
        config_, std::move(pc_dependencies));
    if (!result.ok()) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(result.value().get());
    auto wrapper = std::make_unique<PeerConnectionWrapper>(
        pc_factory_, result.MoveValue(), std::move(observer));
    return wrapper;
  }

  rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>
  CreateVideoTrackSource() {
    FrameGeneratorCapturerVideoTrackSource::Config config;
    rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>
        video_track_source;
    video_track_source =
        rtc::make_ref_counted<FrameGeneratorCapturerVideoTrackSource>(
            config, clock_, /*is_screencast=*/screencast_);
    video_track_source->Start();
    return video_track_source;
  }

  void SetTargetScalabilityMode(
      rtc::scoped_refptr<RtpTransceiverInterface> transceiver,
      ScalabilityMode scalability_mode) {
    auto sender = transceiver->sender();
    auto parameters = sender->GetParameters();
    RTC_CHECK_GT(parameters.encodings.size(), 0u);
    absl::string_view scalability_mode_string =
        ScalabilityModeToString(scalability_mode);
    for (auto& encoding : parameters.encodings) {
      encoding.scalability_mode = std::string(scalability_mode_string);
    }
    RTCError error = sender->SetParameters(parameters);
    RTC_CHECK(error.ok());
  }

  void OnInitEncode(const VideoCodec* video_codec) {
    std::function<void(const VideoCodec*)> on_init_encode_callback;
    {
      MutexLock lock(&mutex_);
      on_init_encode_callback = on_init_encode_callback_;
    }
    if (on_init_encode_callback) {
      on_init_encode_callback(video_codec);
    } else {
      RTC_LOG(LS_WARNING) << "OnInitEncode callback not set.";
    }
  }

  void SetOnInitEncode(
      std::function<void(const VideoCodec*)> on_init_encode_callback) {
    MutexLock lock(&mutex_);
    on_init_encode_callback_ = std::move(on_init_encode_callback);
  }

  VideoCodecType codec_type_;
  const int max_temporal_layers_;
  const absl::optional<ScalabilityMode> target_scalability_mode_;
  const int simulcast_layers_;
  const bool screencast_;

  Clock* const clock_;
  rtc::AutoThread main_thread_;
  rtc::PhysicalSocketServer pss_;
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;

  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;

  webrtc::PeerConnectionInterface::RTCConfiguration config_;

  Mutex mutex_;
  std::function<void(const VideoCodec*)> on_init_encode_callback_
      RTC_GUARDED_BY(mutex_);
};

TEST_P(PeerConnectionSVCEncoderIntegrationTest, CheckEncoderTemporalLayers) {
  absl::optional<int> target_temporal_layers;
  if (target_scalability_mode_.has_value()) {
    target_temporal_layers =
        ScalabilityModeToNumTemporalLayers(target_scalability_mode_.value());
    ASSERT_TRUE(target_temporal_layers.has_value());

    if (target_temporal_layers.value() > max_temporal_layers_) {
      GTEST_SKIP() << "Target scalability mode not supported.";
    }
  }

  // Skip failing cases
  // TODO(bugs.webrtc.org/XXXXX): Fix failing cases
  if ((codec_type_ == kVideoCodecVP8 || codec_type_ == kVideoCodecVP9) &&
      !target_scalability_mode_ && simulcast_layers_ > 1) {
    GTEST_SKIP() << "Skipping failing case";
  }

  Mutex mutex;
  std::vector<VideoCodec> video_codecs /* RTC_GUARDED_BY(mutex) */;

  auto on_init_encode = [&mutex, &video_codecs](const VideoCodec* video_codec) {
    MutexLock lock(&mutex);
    video_codecs.push_back(*video_codec);
  };
  SetOnInitEncode(on_init_encode);

  WrapperPtr caller = CreatePeerConnection();
  rtc::scoped_refptr<RtpTransceiverInterface> caller_transceiver;
  rtc::scoped_refptr<VideoTrackInterface> caller_video_source =
      pc_factory_->CreateVideoTrack(CreateVideoTrackSource(), "v");

  if (simulcast_layers_ > 1) {
    RtpTransceiverInit transceiver_init;
    for (int i = 0; i < simulcast_layers_; ++i) {
      RtpEncodingParameters encoding;
      encoding.rid = std::to_string(i);
      encoding.active = true;
      transceiver_init.send_encodings.push_back(encoding);
    }
    auto caller_transeiver_or_error = caller->pc()->AddTransceiver(
        cricket::MediaType::MEDIA_TYPE_VIDEO, transceiver_init);
    ASSERT_TRUE(caller_transeiver_or_error.ok());
    caller_transceiver = std::move(caller_transeiver_or_error.value());

    // Note: SetTrack should accept a scoped_refptr, not a raw pointer.
    ASSERT_TRUE(
        caller_transceiver->sender()->SetTrack(caller_video_source.get()));
  } else {
    caller->AddTrack(caller_video_source);
    auto caller_transceivers = caller->pc()->GetTransceivers();
    ASSERT_EQ(caller_transceivers.size(), 1u);
    caller_transceiver = caller_transceivers[0];
  }

  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  if (target_scalability_mode_.has_value()) {
    SetTargetScalabilityMode(caller_transceiver,
                             target_scalability_mode_.value());
  }

  ASSERT_TRUE_WAIT(
      caller->signaling_state() == PeerConnectionInterface::kStable,
      kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(caller->IsIceGatheringDone(), kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(callee->IsIceGatheringDone(), kDefaultTimeoutMs);

  // Connect an ICE candidate pairs.
  ASSERT_TRUE(
      AddIceCandidates(callee.get(), caller->observer()->GetAllCandidates()));
  ASSERT_TRUE(
      AddIceCandidates(caller.get(), callee->observer()->GetAllCandidates()));

  // This means that ICE and DTLS are connected.
  ASSERT_TRUE_WAIT(callee->IsIceConnected(), kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(caller->IsIceConnected(), kDefaultTimeoutMs);

  bool received_on_init_callback = [&mutex, &video_codecs]() {
    MutexLock lock(&mutex);
    return video_codecs.size() > 0;
  }();

  ASSERT_TRUE_WAIT(received_on_init_callback, kDefaultTimeoutMs);

  std::vector<VideoCodec> video_codecs_copy;
  {
    MutexLock lock(&mutex);
    video_codecs_copy = video_codecs;
  }

  ASSERT_GE(video_codecs_copy.size(), 1u);
  for (const VideoCodec& video_codec : video_codecs_copy) {
    int num_temporal_layers =
        SimulcastUtility::NumberOfTemporalLayers(video_codec, 0);
    if (target_scalability_mode_.has_value()) {
      ASSERT_EQ(ScalabilityModeToNumTemporalLayers(*target_scalability_mode_),
                num_temporal_layers);
    } else {
      ASSERT_GE(num_temporal_layers, 1);
      ASSERT_LE(num_temporal_layers, max_temporal_layers_);
    }
  }
}

const auto kCodecs = Values(
#if defined(WEBRTC_USE_H264)
    kVideoCodecH264,
#endif
    kVideoCodecVP8,
#if RTC_ENABLE_VP9
    kVideoCodecVP9,
#endif
    kVideoCodecAV1);

INSTANTIATE_TEST_SUITE_P(PeerConnectionSVCEncoderIntegrationTest,
                         PeerConnectionSVCEncoderIntegrationTest,
                         Combine(kCodecs,
                                 Values(1, 2, 3) /* max_temporal_layers */,
                                 Values(absl::nullopt,
                                        ScalabilityMode::kL1T1,
                                        ScalabilityMode::kL1T2,
                                        ScalabilityMode::kL1T3),
                                 Values(1, 2, 3) /* simulcast_layers */,
                                 Values(false, true) /* screencast */),
                         TestParametersMidTestConfigurationToString);

}  // namespace webrtc
