/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/call_test.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/rtp_transport_controller_send.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "test/fake_encoder.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
const int kVideoRotationRtpExtensionId = 4;
}

CallTest::CallTest()
    : clock_(Clock::GetRealTimeClock()),
      event_log_(RtcEventLog::CreateNull()),
      sender_call_transport_controller_(nullptr),
      audio_send_config_(nullptr),
      audio_send_stream_(nullptr),
      fake_encoder_factory_([this]() {
        auto encoder = absl::make_unique<test::FakeEncoder>(clock_);
        encoder->SetMaxBitrate(fake_encoder_max_bitrate_);
        return encoder;
      }),
      num_video_streams_(1),
      num_audio_streams_(0),
      num_flexfec_streams_(0),
      audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()),
      audio_encoder_factory_(CreateBuiltinAudioEncoderFactory()),
      task_queue_("CallTestTaskQueue") {}

CallTest::~CallTest() {
  task_queue_.SendTask([this]() {
    fake_send_audio_device_ = nullptr;
    fake_recv_audio_device_ = nullptr;
    allocated_capturers_.clear();
  });
}

void CallTest::RunBaseTest(BaseTest* test) {
  task_queue_.SendTask([this, test]() {
    num_video_streams_ = test->GetNumVideoStreams();
    num_audio_streams_ = test->GetNumAudioStreams();
    num_flexfec_streams_ = test->GetNumFlexfecStreams();
    RTC_DCHECK(num_video_streams_ > 0 || num_audio_streams_ > 0);
    Call::Config send_config(test->GetSenderCallConfig());
    if (num_audio_streams_ > 0) {
      CreateFakeAudioDevices(test->CreateCapturer(), test->CreateRenderer());
      test->OnFakeAudioDevicesCreated(fake_send_audio_device_.get(),
                                      fake_recv_audio_device_.get());
      apm_send_ = AudioProcessingBuilder().Create();
      apm_recv_ = AudioProcessingBuilder().Create();
      EXPECT_EQ(0, fake_send_audio_device_->Init());
      EXPECT_EQ(0, fake_recv_audio_device_->Init());
      AudioState::Config audio_state_config;
      audio_state_config.audio_mixer = AudioMixerImpl::Create();
      audio_state_config.audio_processing = apm_send_;
      audio_state_config.audio_device_module = fake_send_audio_device_;
      send_config.audio_state = AudioState::Create(audio_state_config);
      fake_send_audio_device_->RegisterAudioCallback(
          send_config.audio_state->audio_transport());
    }
    CreateSenderCall(send_config);
    if (sender_call_transport_controller_ != nullptr) {
      test->OnRtpTransportControllerSendCreated(
          sender_call_transport_controller_);
    }
    if (test->ShouldCreateReceivers()) {
      Call::Config recv_config(test->GetReceiverCallConfig());
      if (num_audio_streams_ > 0) {
        AudioState::Config audio_state_config;
        audio_state_config.audio_mixer = AudioMixerImpl::Create();
        audio_state_config.audio_processing = apm_recv_;
        audio_state_config.audio_device_module = fake_recv_audio_device_;
        recv_config.audio_state = AudioState::Create(audio_state_config);
        fake_recv_audio_device_->RegisterAudioCallback(
            recv_config.audio_state->audio_transport());
      }
      CreateReceiverCall(recv_config);
    }
    test->OnCallsCreated(sender_call_.get(), receiver_call_.get());
    receive_transport_.reset(test->CreateReceiveTransport(&task_queue_));
    send_transport_.reset(
        test->CreateSendTransport(&task_queue_, sender_call_.get()));

    if (test->ShouldCreateReceivers()) {
      send_transport_->SetReceiver(receiver_call_->Receiver());
      receive_transport_->SetReceiver(sender_call_->Receiver());
      if (num_video_streams_ > 0)
        receiver_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
      if (num_audio_streams_ > 0)
        receiver_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    } else {
      // Sender-only call delivers to itself.
      send_transport_->SetReceiver(sender_call_->Receiver());
      receive_transport_->SetReceiver(nullptr);
    }

    CreateSendConfig(num_video_streams_, num_audio_streams_,
                     num_flexfec_streams_, send_transport_.get());
    if (test->ShouldCreateReceivers()) {
      CreateMatchingReceiveConfigs(receive_transport_.get());
    }
    if (num_video_streams_ > 0) {
      test->ModifyVideoConfigs(&GetVideoSendConfig(), &video_receive_configs_,
                               &GetVideoEncoderConfig());
    }
    if (num_audio_streams_ > 0) {
      test->ModifyAudioConfigs(&audio_send_config_, &audio_receive_configs_);
    }
    if (num_flexfec_streams_ > 0) {
      test->ModifyFlexfecConfigs(&flexfec_receive_configs_);
    }

    if (num_flexfec_streams_ > 0) {
      CreateFlexfecStreams();
      test->OnFlexfecStreamsCreated(flexfec_receive_streams_);
    }
    if (num_video_streams_ > 0) {
      CreateVideoStreams();
      test->OnVideoStreamsCreated(GetVideoSendStream(), video_receive_streams_);
    }
    if (num_audio_streams_ > 0) {
      CreateAudioStreams();
      test->OnAudioStreamsCreated(audio_send_stream_, audio_receive_streams_);
    }

    if (num_video_streams_ > 0) {
      int width = kDefaultWidth;
      int height = kDefaultHeight;
      int frame_rate = kDefaultFramerate;
      test->ModifyVideoCaptureStartResolution(&width, &height, &frame_rate);
      CreateFrameGeneratorCapturer(frame_rate, width, height);
      test->OnFrameGeneratorCapturerCreated(frame_generator_capturer_);
    }

    Start();
  });

  test->PerformTest();

  task_queue_.SendTask([this, test]() {
    Stop();
    test->OnStreamsStopped();
    DestroyStreams();
    send_transport_.reset();
    receive_transport_.reset();
    DestroyCalls();
  });
}

void CallTest::CreateCalls(const Call::Config& sender_config,
                           const Call::Config& receiver_config) {
  CreateSenderCall(sender_config);
  CreateReceiverCall(receiver_config);
}

void CallTest::CreateSenderCall(const Call::Config& config) {
  std::unique_ptr<RtpTransportControllerSend> controller_send =
      absl::make_unique<RtpTransportControllerSend>(
          Clock::GetRealTimeClock(), config.event_log,
          config.network_controller_factory, config.bitrate_config);
  sender_call_transport_controller_ = controller_send.get();
  sender_call_.reset(Call::Create(config, std::move(controller_send)));
}

void CallTest::CreateReceiverCall(const Call::Config& config) {
  receiver_call_.reset(Call::Create(config));
}

void CallTest::DestroyCalls() {
  sender_call_.reset();
  receiver_call_.reset();
}

void CallTest::CreateVideoSendConfig() {
  if (num_video_streams_ > 0) {
    video_send_configs_.clear();
    VideoSendStream::Config config(nullptr);
    CreateVideoSendConfig(&config, num_video_streams_, 0,
                          send_transport_.get());
    video_send_configs_.push_back(config.Copy());
  }
}

void CallTest::CreateVideoSendConfig(VideoSendStream::Config* video_config,
                                     size_t num_video_streams,
                                     size_t num_used_ssrcs,
                                     Transport* send_transport) {
  RTC_DCHECK_LE(num_video_streams + num_used_ssrcs, kNumSsrcs);
  *video_config = VideoSendStream::Config(send_transport);
  video_config->encoder_settings.encoder_factory = &fake_encoder_factory_;
  video_config->rtp.payload_name = "FAKE";
  video_config->rtp.payload_type = kFakeVideoSendPayloadType;
  video_config->rtp.extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                   kTransportSequenceNumberExtensionId));
  video_config->rtp.extensions.push_back(RtpExtension(
      RtpExtension::kVideoContentTypeUri, kVideoContentTypeExtensionId));
  if (video_encoder_configs_.empty()) {
    video_encoder_configs_.emplace_back();
    FillEncoderConfiguration(kVideoCodecGeneric, num_video_streams,
                             &video_encoder_configs_.back());
  }

  for (size_t i = 0; i < num_video_streams; ++i)
    video_config->rtp.ssrcs.push_back(kVideoSendSsrcs[num_used_ssrcs + i]);
  video_config->rtp.extensions.push_back(RtpExtension(
      RtpExtension::kVideoRotationUri, kVideoRotationRtpExtensionId));
}

void CallTest::CreateAudioAndFecSendConfigs(size_t num_audio_streams,
                                            size_t num_flexfec_streams,
                                            Transport* send_transport) {
  RTC_DCHECK_LE(num_audio_streams, 1);
  RTC_DCHECK_LE(num_flexfec_streams, 1);
  if (num_audio_streams > 0) {
    audio_send_config_ = AudioSendStream::Config(send_transport);
    audio_send_config_.rtp.ssrc = kAudioSendSsrc;
    audio_send_config_.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
        kAudioSendPayloadType, {"opus", 48000, 2, {{"stereo", "1"}}});
    audio_send_config_.encoder_factory = audio_encoder_factory_;
  }

  // TODO(brandtr): Update this when we support multistream protection.
  if (num_flexfec_streams > 0) {
    GetVideoSendConfig().rtp.flexfec.payload_type = kFlexfecPayloadType;
    GetVideoSendConfig().rtp.flexfec.ssrc = kFlexfecSendSsrc;
    GetVideoSendConfig().rtp.flexfec.protected_media_ssrcs = {
        kVideoSendSsrcs[0]};
  }
}

void CallTest::CreateSendConfig(size_t num_video_streams,
                                size_t num_audio_streams,
                                size_t num_flexfec_streams,
                                Transport* send_transport) {
  if (num_video_streams > 0) {
    video_send_configs_.clear();
    video_send_configs_.emplace_back(nullptr);
    CreateVideoSendConfig(&video_send_configs_.back(), num_video_streams, 0,
                          send_transport);
  }
  CreateAudioAndFecSendConfigs(num_audio_streams, num_flexfec_streams,
                               send_transport);
}

std::vector<VideoReceiveStream::Config>
CallTest::CreateMatchingVideoReceiveConfigs(
    const VideoSendStream::Config& video_send_config,
    Transport* rtcp_send_transport) {
  return CreateMatchingVideoReceiveConfigs(
      video_send_config, rtcp_send_transport, true, absl::nullopt, false, 0);
}

std::vector<VideoReceiveStream::Config>
CallTest::CreateMatchingVideoReceiveConfigs(
    const VideoSendStream::Config& video_send_config,
    Transport* rtcp_send_transport,
    bool send_side_bwe,
    absl::optional<size_t> decode_sub_stream,
    bool receiver_reference_time_report,
    int rtp_history_ms) {
  RTC_DCHECK(!video_send_config.rtp.ssrcs.empty());
  VideoReceiveStream::Config default_config(rtcp_send_transport);
  default_config.rtp.remb = !send_side_bwe;
  default_config.rtp.transport_cc = send_side_bwe;
  default_config.rtp.local_ssrc = kReceiverLocalVideoSsrc;
  for (const RtpExtension& extension : video_send_config.rtp.extensions)
    default_config.rtp.extensions.push_back(extension);
  default_config.rtp.nack.rtp_history_ms = rtp_history_ms;
  // Enable RTT calculation so NTP time estimator will work.
  default_config.rtp.rtcp_xr.receiver_reference_time_report =
      receiver_reference_time_report;
  default_config.renderer = &fake_renderer_;

  std::vector<VideoReceiveStream::Config> result;

  for (size_t i = 0; i < video_send_config.rtp.ssrcs.size(); ++i) {
    VideoReceiveStream::Config video_recv_config(default_config.Copy());
    video_recv_config.decoders.clear();
    if (!video_send_config.rtp.rtx.ssrcs.empty()) {
      video_recv_config.rtp.rtx_ssrc = video_send_config.rtp.rtx.ssrcs[i];
      video_recv_config.rtp.rtx_associated_payload_types[kSendRtxPayloadType] =
          video_send_config.rtp.payload_type;
    }
    video_recv_config.rtp.remote_ssrc = video_send_config.rtp.ssrcs[i];
    VideoReceiveStream::Decoder decoder;

    // Force fake decoders on non-selected simulcast streams.
    if (!decode_sub_stream || i == *decode_sub_stream) {
      decoder = test::CreateMatchingDecoder(video_send_config);
    } else {
      decoder.decoder = new test::FakeDecoder();
      decoder.payload_type = video_send_config.rtp.payload_type;
      decoder.payload_name = video_send_config.rtp.payload_name;
    }
    allocated_decoders_.emplace_back(decoder.decoder);
    video_recv_config.decoders.push_back(decoder);
    result.emplace_back(std::move(video_recv_config));
  }
  return result;
}

void CallTest::CreateMatchingAudioAndFecConfigs(
    Transport* rtcp_send_transport) {
  RTC_DCHECK_GE(1, num_audio_streams_);
  if (num_audio_streams_ == 1) {
    AudioReceiveStream::Config audio_config;
    audio_config.rtp.local_ssrc = kReceiverLocalAudioSsrc;
    audio_config.rtcp_send_transport = rtcp_send_transport;
    audio_config.rtp.remote_ssrc = audio_send_config_.rtp.ssrc;
    audio_config.decoder_factory = audio_decoder_factory_;
    audio_config.decoder_map = {{kAudioSendPayloadType, {"opus", 48000, 2}}};
    audio_receive_configs_.push_back(audio_config);
  }

  // TODO(brandtr): Update this when we support multistream protection.
  RTC_DCHECK(num_flexfec_streams_ <= 1);
  if (num_flexfec_streams_ == 1) {
    FlexfecReceiveStream::Config config(rtcp_send_transport);
    config.payload_type = kFlexfecPayloadType;
    config.remote_ssrc = kFlexfecSendSsrc;
    config.protected_media_ssrcs = {kVideoSendSsrcs[0]};
    config.local_ssrc = kReceiverLocalVideoSsrc;
    for (const RtpExtension& extension : GetVideoSendConfig().rtp.extensions)
      config.rtp_header_extensions.push_back(extension);
    flexfec_receive_configs_.push_back(config);
  }
}

void CallTest::CreateMatchingReceiveConfigs(Transport* rtcp_send_transport) {
  video_receive_configs_.clear();
  allocated_decoders_.clear();
  for (VideoSendStream::Config& video_send_config : video_send_configs_) {
    std::vector<VideoReceiveStream::Config> new_configs =
        CreateMatchingVideoReceiveConfigs(video_send_config,
                                          rtcp_send_transport);
    for (VideoReceiveStream::Config& config : new_configs) {
      video_receive_configs_.push_back(config.Copy());
    }
  }
  if (!video_receive_configs_.empty() && num_flexfec_streams_ == 1) {
    video_receive_configs_[0].rtp.protected_by_flexfec = true;
  }
  CreateMatchingAudioAndFecConfigs(rtcp_send_transport);
}

void CallTest::CreateFrameGeneratorCapturerWithDrift(Clock* clock,
                                                     float speed,
                                                     int framerate,
                                                     int width,
                                                     int height) {
  RTC_DCHECK(video_sources_.empty());
  frame_generator_capturer_ = test::FrameGeneratorCapturer::Create(
      width, height, absl::nullopt, absl::nullopt, framerate * speed, clock);
  allocated_capturers_.emplace_back(
      std::unique_ptr<FrameGeneratorCapturer>(frame_generator_capturer_));
  video_sources_.push_back(frame_generator_capturer_);
  video_capturers_.push_back(frame_generator_capturer_);
  ConnectVideoSourcesToStreams();
}

void CallTest::CreateFrameGeneratorCapturer(int framerate,
                                            int width,
                                            int height) {
  RTC_DCHECK(video_sources_.empty());
  frame_generator_capturer_ = test::FrameGeneratorCapturer::Create(
      width, height, absl::nullopt, absl::nullopt, framerate, clock_);
  allocated_capturers_.emplace_back(
      std::unique_ptr<FrameGeneratorCapturer>(frame_generator_capturer_));
  video_sources_.push_back(frame_generator_capturer_);
  video_capturers_.push_back(frame_generator_capturer_);
  ConnectVideoSourcesToStreams();
}

void CallTest::CreateFakeAudioDevices(
    std::unique_ptr<TestAudioDeviceModule::Capturer> capturer,
    std::unique_ptr<TestAudioDeviceModule::Renderer> renderer) {
  fake_send_audio_device_ = TestAudioDeviceModule::CreateTestAudioDeviceModule(
      std::move(capturer), nullptr, 1.f);
  fake_recv_audio_device_ = TestAudioDeviceModule::CreateTestAudioDeviceModule(
      nullptr, std::move(renderer), 1.f);
}

void CallTest::CreateVideoStreams() {
  RTC_DCHECK(video_receive_streams_.empty());
  CreateVideoSendStreams();
  for (size_t i = 0; i < video_receive_configs_.size(); ++i) {
    video_receive_streams_.push_back(receiver_call_->CreateVideoReceiveStream(
        video_receive_configs_[i].Copy()));
  }

  AssociateFlexfecStreamsWithVideoStreams();
}

void CallTest::CreateVideoSendStreams() {
  RTC_DCHECK(video_send_streams_.empty());
  for (size_t i = 0; i < video_send_configs_.size(); ++i) {
    video_send_streams_.push_back(sender_call_->CreateVideoSendStream(
        video_send_configs_[i].Copy(), video_encoder_configs_[i].Copy()));
  }
}

void CallTest::CreateVideoSendStream(const VideoEncoderConfig& encoder_config) {
  RTC_DCHECK(video_send_streams_.empty());
  video_send_streams_.push_back(sender_call_->CreateVideoSendStream(
      GetVideoSendConfig().Copy(), encoder_config.Copy()));
}

void CallTest::CreateAudioStreams() {
  RTC_DCHECK(audio_send_stream_ == nullptr);
  RTC_DCHECK(audio_receive_streams_.empty());
  audio_send_stream_ = sender_call_->CreateAudioSendStream(audio_send_config_);
  for (size_t i = 0; i < audio_receive_configs_.size(); ++i) {
    audio_receive_streams_.push_back(
        receiver_call_->CreateAudioReceiveStream(audio_receive_configs_[i]));
  }
}

void CallTest::CreateFlexfecStreams() {
  for (size_t i = 0; i < flexfec_receive_configs_.size(); ++i) {
    flexfec_receive_streams_.push_back(
        receiver_call_->CreateFlexfecReceiveStream(
            flexfec_receive_configs_[i]));
  }

  AssociateFlexfecStreamsWithVideoStreams();
}

void CallTest::ConnectVideoSourcesToStreams() {
  for (size_t i = 0; i < video_sources_.size(); ++i)
    video_send_streams_[i]->SetSource(video_sources_[i],
                                      degradation_preference_);
}

void CallTest::AssociateFlexfecStreamsWithVideoStreams() {
  // All FlexFEC streams protect all of the video streams.
  for (FlexfecReceiveStream* flexfec_recv_stream : flexfec_receive_streams_) {
    for (VideoReceiveStream* video_recv_stream : video_receive_streams_) {
      video_recv_stream->AddSecondarySink(flexfec_recv_stream);
    }
  }
}

void CallTest::DissociateFlexfecStreamsFromVideoStreams() {
  for (FlexfecReceiveStream* flexfec_recv_stream : flexfec_receive_streams_) {
    for (VideoReceiveStream* video_recv_stream : video_receive_streams_) {
      video_recv_stream->RemoveSecondarySink(flexfec_recv_stream);
    }
  }
}

void CallTest::Start() {
  StartVideoStreams();
  if (audio_send_stream_) {
    audio_send_stream_->Start();
  }
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Start();
  StartVideoCapture();
}

void CallTest::StartVideoStreams() {
  for (VideoSendStream* video_send_stream : video_send_streams_)
    video_send_stream->Start();
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    video_recv_stream->Start();
}

void CallTest::StartVideoCapture() {
  for (auto& capturer : video_capturers_)
    capturer->Start();
}

void CallTest::Stop() {
  for (auto& capturer : video_capturers_)
    capturer->Stop();
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Stop();
  if (audio_send_stream_) {
    audio_send_stream_->Stop();
  }
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    video_recv_stream->Stop();
  for (VideoSendStream* video_send_stream : video_send_streams_)
    video_send_stream->Stop();
}

void CallTest::DestroyStreams() {
  DissociateFlexfecStreamsFromVideoStreams();

  if (audio_send_stream_)
    sender_call_->DestroyAudioSendStream(audio_send_stream_);
  audio_send_stream_ = nullptr;
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    receiver_call_->DestroyAudioReceiveStream(audio_recv_stream);

  DestroyVideoSendStreams();

  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    receiver_call_->DestroyVideoReceiveStream(video_recv_stream);

  for (FlexfecReceiveStream* flexfec_recv_stream : flexfec_receive_streams_)
    receiver_call_->DestroyFlexfecReceiveStream(flexfec_recv_stream);

  video_receive_streams_.clear();
  allocated_decoders_.clear();
}

void CallTest::DestroyVideoSendStreams() {
  for (VideoSendStream* video_send_stream : video_send_streams_)
    sender_call_->DestroyVideoSendStream(video_send_stream);
  video_send_streams_.clear();
}

void CallTest::SetFakeVideoCaptureRotation(VideoRotation rotation) {
  frame_generator_capturer_->SetFakeRotation(rotation);
}

void CallTest::SetVideoSource(
    rtc::VideoSourceInterface<VideoFrame>* source,
    absl::optional<DegradationPreference> preference) {
  if (source != nullptr) {
    source = frame_generator_capturer_;
  }
  if (!preference) {
    preference = degradation_preference_;
  }
  GetVideoSendStream()->SetSource(source, *preference);
}

void CallTest::SetVideoSource(rtc::VideoSourceInterface<VideoFrame>* source) {
  RTC_DCHECK(source);
  SetVideoSource(source, absl::nullopt);
}

void CallTest::SetVideoDegradation(DegradationPreference preference) {
  SetVideoSource(nullptr, preference);
}

VideoSendStream::Config& CallTest::GetVideoSendConfig() {
  return video_send_configs_[0];
}

VideoEncoderConfig& CallTest::GetVideoEncoderConfig() {
  return video_encoder_configs_[0];
}

VideoSendStream* CallTest::GetVideoSendStream() {
  return video_send_streams_[0];
}

constexpr size_t CallTest::kNumSsrcs;
const int CallTest::kDefaultWidth;
const int CallTest::kDefaultHeight;
const int CallTest::kDefaultFramerate;
const int CallTest::kDefaultTimeoutMs = 30 * 1000;
const int CallTest::kLongTimeoutMs = 120 * 1000;
const uint32_t CallTest::kSendRtxSsrcs[kNumSsrcs] = {
    0xBADCAFD, 0xBADCAFE, 0xBADCAFF, 0xBADCB00, 0xBADCB01, 0xBADCB02};
const uint32_t CallTest::kVideoSendSsrcs[kNumSsrcs] = {
    0xC0FFED, 0xC0FFEE, 0xC0FFEF, 0xC0FFF0, 0xC0FFF1, 0xC0FFF2};
const uint32_t CallTest::kAudioSendSsrc = 0xDEADBEEF;
const uint32_t CallTest::kFlexfecSendSsrc = 0xBADBEEF;
const uint32_t CallTest::kReceiverLocalVideoSsrc = 0x123456;
const uint32_t CallTest::kReceiverLocalAudioSsrc = 0x1234567;
const int CallTest::kNackRtpHistoryMs = 1000;

const uint8_t CallTest::kDefaultKeepalivePayloadType =
    RtpKeepAliveConfig().payload_type;

const std::map<uint8_t, MediaType> CallTest::payload_type_map_ = {
    {CallTest::kVideoSendPayloadType, MediaType::VIDEO},
    {CallTest::kFakeVideoSendPayloadType, MediaType::VIDEO},
    {CallTest::kSendRtxPayloadType, MediaType::VIDEO},
    {CallTest::kRedPayloadType, MediaType::VIDEO},
    {CallTest::kRtxRedPayloadType, MediaType::VIDEO},
    {CallTest::kUlpfecPayloadType, MediaType::VIDEO},
    {CallTest::kFlexfecPayloadType, MediaType::VIDEO},
    {CallTest::kAudioSendPayloadType, MediaType::AUDIO},
    {CallTest::kDefaultKeepalivePayloadType, MediaType::ANY}};

BaseTest::BaseTest() : event_log_(RtcEventLog::CreateNull()) {}

BaseTest::BaseTest(unsigned int timeout_ms)
    : RtpRtcpObserver(timeout_ms), event_log_(RtcEventLog::CreateNull()) {}

BaseTest::~BaseTest() {}

std::unique_ptr<TestAudioDeviceModule::Capturer> BaseTest::CreateCapturer() {
  return TestAudioDeviceModule::CreatePulsedNoiseCapturer(256, 48000);
}

std::unique_ptr<TestAudioDeviceModule::Renderer> BaseTest::CreateRenderer() {
  return TestAudioDeviceModule::CreateDiscardRenderer(48000);
}

void BaseTest::OnFakeAudioDevicesCreated(
    TestAudioDeviceModule* send_audio_device,
    TestAudioDeviceModule* recv_audio_device) {}

Call::Config BaseTest::GetSenderCallConfig() {
  return Call::Config(event_log_.get());
}

Call::Config BaseTest::GetReceiverCallConfig() {
  return Call::Config(event_log_.get());
}

void BaseTest::OnRtpTransportControllerSendCreated(
    RtpTransportControllerSend* controller) {}

void BaseTest::OnCallsCreated(Call* sender_call, Call* receiver_call) {}

test::PacketTransport* BaseTest::CreateSendTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    Call* sender_call) {
  return new PacketTransport(
      task_queue, sender_call, this, test::PacketTransport::kSender,
      CallTest::payload_type_map_, FakeNetworkPipe::Config());
}

test::PacketTransport* BaseTest::CreateReceiveTransport(
    SingleThreadedTaskQueueForTesting* task_queue) {
  return new PacketTransport(
      task_queue, nullptr, this, test::PacketTransport::kReceiver,
      CallTest::payload_type_map_, FakeNetworkPipe::Config());
}

size_t BaseTest::GetNumVideoStreams() const {
  return 1;
}

size_t BaseTest::GetNumAudioStreams() const {
  return 0;
}

size_t BaseTest::GetNumFlexfecStreams() const {
  return 0;
}

void BaseTest::ModifyVideoConfigs(
    VideoSendStream::Config* send_config,
    std::vector<VideoReceiveStream::Config>* receive_configs,
    VideoEncoderConfig* encoder_config) {}

void BaseTest::ModifyVideoCaptureStartResolution(int* width,
                                                 int* heigt,
                                                 int* frame_rate) {}

void BaseTest::OnVideoStreamsCreated(
    VideoSendStream* send_stream,
    const std::vector<VideoReceiveStream*>& receive_streams) {}

void BaseTest::ModifyAudioConfigs(
    AudioSendStream::Config* send_config,
    std::vector<AudioReceiveStream::Config>* receive_configs) {}

void BaseTest::OnAudioStreamsCreated(
    AudioSendStream* send_stream,
    const std::vector<AudioReceiveStream*>& receive_streams) {}

void BaseTest::ModifyFlexfecConfigs(
    std::vector<FlexfecReceiveStream::Config>* receive_configs) {}

void BaseTest::OnFlexfecStreamsCreated(
    const std::vector<FlexfecReceiveStream*>& receive_streams) {}

void BaseTest::OnFrameGeneratorCapturerCreated(
    FrameGeneratorCapturer* frame_generator_capturer) {}

void BaseTest::OnStreamsStopped() {}

SendTest::SendTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {}

bool SendTest::ShouldCreateReceivers() const {
  return false;
}

EndToEndTest::EndToEndTest() {}

EndToEndTest::EndToEndTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {}

bool EndToEndTest::ShouldCreateReceivers() const {
  return true;
}

}  // namespace test
}  // namespace webrtc
