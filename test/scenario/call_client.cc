/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/call_client.h"

#include <utility>

#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/congestion_controller/bbr/test/bbr_printer.h"
#include "modules/congestion_controller/goog_cc/test/goog_cc_printer.h"
#include "test/call_test.h"

namespace webrtc {
namespace test {

void CallClient::DeliverPacket(MediaType media_type,
                               rtc::CopyOnWriteBuffer packet) {
  PacketTime packet_time(clock_->TimeInMicroseconds(), -1);
  call_->Receiver()->DeliverPacket(media_type, packet, packet_time);
}

LambdaPrinter CallClient::StatsPrinter() {
  return {"pacer_delay call_send_bw",
          [this](rtc::SimpleStringBuilder& sb) {
            Call::Stats call_stats = call_->GetStats();
            sb.AppendFormat("%.3lf %.0lf", call_stats.pacer_delay_ms / 1000.0,
                            call_stats.send_bandwidth_bps / 8.0);
          },
          64};
}

uint32_t CallClient::GetNextVideoSsrc() {
  RTC_CHECK_LT(next_video_ssrc_, CallTest::kNumSsrcs);
  return CallTest::kVideoSendSsrcs[next_video_ssrc_++];
}

uint32_t CallClient::GetNextRtxSsrc() {
  RTC_CHECK_LT(next_rtx_ssrc_, CallTest::kNumSsrcs);
  return CallTest::kSendRtxSsrcs[next_rtx_ssrc_++];
}

CallClient::CallClient(std::string name,
                       CallClientConfig config,
                       std::string base_filename)
    : clock_(Clock::GetRealTimeClock()) {
  if (!base_filename.empty()) {
    std::string filename = base_filename + "." + name;
    event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::Legacy);
    bool success = event_log_->StartLogging(
        absl::make_unique<RtcEventLogOutputFile>(filename + ".rtc.dat",
                                                 RtcEventLog::kUnlimitedOutput),
        RtcEventLog::kImmediateOutput);
    RTC_CHECK(success);
    cc_out_ = fopen((filename + ".ccstate.txt").c_str(), "w");
    if (config.cc.type == CallClientConfig::CongestionControl::Type::kBbr) {
      auto bbr_printer = absl::make_unique<BbrStatePrinter>();
      cc_factory_.reset(new BbrDebugFactory(bbr_printer.get()));
      cc_printer_.reset(
          new ControlStatePrinter(cc_out_, std::move(bbr_printer)));
    } else {
      auto goog_printer = absl::make_unique<GoogCcStatePrinter>();
      cc_factory_.reset(
          new GoogCcDebugFactory(event_log_.get(), goog_printer.get()));
      cc_printer_.reset(
          new ControlStatePrinter(cc_out_, std::move(goog_printer)));
    }
    cc_printer_->PrintHeaders();
  } else {
    event_log_ = RtcEventLog::CreateNull();
    if (config.cc.type == CallClientConfig::CongestionControl::Type::kBbr)
      cc_factory_.reset(new BbrNetworkControllerFactory());
  }
  CallConfig call_config(event_log_.get());
  if (config.rates.max_rate)
    call_config.bitrate_config.max_bitrate_bps = config.rates.max_rate->bps();
  if (config.rates.max_rate)
    call_config.bitrate_config.min_bitrate_bps = config.rates.min_rate->bps();
  if (config.rates.max_rate)
    call_config.bitrate_config.start_bitrate_bps =
        config.rates.start_rate->bps();
  call_config.network_controller_factory = cc_factory_.get();
  call_config.audio_state = InitAudio();
  call_.reset(Call::Create(call_config));
}

CallClient::~CallClient() {
  if (cc_out_)
    fclose(cc_out_);
}

rtc::scoped_refptr<AudioState> CallClient::InitAudio() {
  auto capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(256, 48000);
  auto renderer = TestAudioDeviceModule::CreateDiscardRenderer(48000);
  fake_audio_device_ = TestAudioDeviceModule::CreateTestAudioDeviceModule(
      std::move(capturer), std::move(renderer), 1.f);
  apm_ = AudioProcessingBuilder().Create();
  fake_audio_device_->Init();
  AudioState::Config audio_state_config;
  audio_state_config.audio_mixer = AudioMixerImpl::Create();
  audio_state_config.audio_processing = apm_;
  audio_state_config.audio_device_module = fake_audio_device_;
  auto audio_state = AudioState::Create(audio_state_config);
  fake_audio_device_->RegisterAudioCallback(audio_state->audio_transport());
  return audio_state;
}
}  // namespace test
}  // namespace webrtc
