/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "absl/memory/memory.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/test/simulated_network.h"
#include "audio/test/audio_end_to_end_test.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "rtc_base/flags.h"
#include "system_wrappers/include/sleep.h"
#include "test/audio_decoder_proxy_factory.h"
#include "test/testsupport/file_utils.h"

WEBRTC_DEFINE_int(sample_rate_hz,
                  16000,
                  "Sample rate (Hz) of the produced audio files.");

WEBRTC_DEFINE_bool(
    quick,
    false,
    "Don't do the full audio recording. "
    "Used to quickly check that the test runs without crashing.");

namespace webrtc {
namespace test {
namespace {

std::string FileSampleRateSuffix() {
  return std::to_string(FLAG_sample_rate_hz / 1000);
}

class AudioQualityTest : public AudioEndToEndTest {
 public:
  AudioQualityTest() = default;

 private:
  std::string AudioInputFile() const {
    return test::ResourcePath(
        "voice_engine/audio_tiny" + FileSampleRateSuffix(), "wav");
  }

  std::string AudioOutputFile() const {
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    return webrtc::test::OutputPath() + "LowBandwidth_" + test_info->name() +
           "_" + FileSampleRateSuffix() + ".wav";
  }

  std::unique_ptr<TestAudioDeviceModule::Capturer> CreateCapturer() override {
    return TestAudioDeviceModule::CreateWavFileReader(AudioInputFile());
  }

  std::unique_ptr<TestAudioDeviceModule::Renderer> CreateRenderer() override {
    return TestAudioDeviceModule::CreateBoundedWavFileWriter(
        AudioOutputFile(), FLAG_sample_rate_hz);
  }

  void PerformTest() override {
    if (FLAG_quick) {
      // Let the recording run for a small amount of time to check if it works.
      SleepMs(1000);
    } else {
      AudioEndToEndTest::PerformTest();
    }
  }

  void OnStreamsStopped() override {
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();

    // Output information about the input and output audio files so that further
    // processing can be done by an external process.
    printf("TEST %s %s %s\n", test_info->name(), AudioInputFile().c_str(),
           AudioOutputFile().c_str());
  }
};

class Mobile2GNetworkTest : public AudioQualityTest {
  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override {
    send_config->send_codec_spec = AudioSendStream::Config::SendCodecSpec(
        test::CallTest::kAudioSendPayloadType,
        {"OPUS",
         48000,
         2,
         {{"maxaveragebitrate", "6000"}, {"ptime", "60"}, {"stereo", "1"}}});
  }

  BuiltInNetworkBehaviorConfig GetNetworkPipeConfig() const override {
    BuiltInNetworkBehaviorConfig pipe_config;
    pipe_config.link_capacity_kbps = 12;
    pipe_config.queue_length_packets = 1500;
    pipe_config.queue_delay_ms = 400;
    return pipe_config;
  }
};

class MultiChannelTest : public AudioQualityTest {
  std::string AudioInputFile() const {
    return webrtc::test::ResourcePath(
        "audio_coding/speech_4_channels_48k_one_second", "wav");
  }

  std::string AudioOutputFile() const {
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    return webrtc::test::OutputPath() + "MultiChannelTest_" +
           test_info->name() + "_" + "48khz" + ".wav";
  }

  std::unique_ptr<TestAudioDeviceModule::Capturer> CreateCapturer() override {
    return TestAudioDeviceModule::CreateWavFileReader(AudioInputFile());
  }

  std::unique_ptr<TestAudioDeviceModule::Renderer> CreateRenderer() override {
    return TestAudioDeviceModule::CreateWavFileWriter(AudioOutputFile(), 48000,
                                                      4);
  }

  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override {
    const auto sdp_format = SdpAudioFormat("opus", 48000, 4, {});
    send_config->send_codec_spec = AudioSendStream::Config::SendCodecSpec(
        test::CallTest::kAudioSendPayloadType, sdp_format);

    const auto decoder_config = AudioDecoderOpus::SdpToConfig(sdp_format);
    opus_decoder_.reset(
        AudioDecoderOpus::MakeAudioDecoder(*decoder_config).release());

    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory =
        new rtc::RefCountedObject<test::AudioDecoderProxyFactory>(
            opus_decoder_.get());

    (*receive_configs)[0].decoder_factory = decoder_factory;
  }
  std::unique_ptr<AudioDecoder> opus_decoder_;
};

}  // namespace

using LowBandwidthAudioTest = CallTest;

TEST_F(LowBandwidthAudioTest, GoodNetworkHighBitrate) {
  AudioQualityTest test;
  RunBaseTest(&test);
}

TEST_F(LowBandwidthAudioTest, Mobile2GNetwork) {
  Mobile2GNetworkTest test;
  RunBaseTest(&test);
}

TEST_F(LowBandwidthAudioTest, MultipleChannelsOpusTest) {
  send_event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::NewFormat);
  recv_event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::NewFormat);
  const std::string dump_name = "rtc_event_log";
  bool event_log_started =
      send_event_log_->StartLogging(
          absl::make_unique<RtcEventLogOutputFile>(
              dump_name + ".send.rtc.dat", RtcEventLog::kUnlimitedOutput),
          RtcEventLog::kImmediateOutput) &&
      recv_event_log_->StartLogging(
          absl::make_unique<RtcEventLogOutputFile>(
              dump_name + ".recv.rtc.dat", RtcEventLog::kUnlimitedOutput),
          RtcEventLog::kImmediateOutput);
  RTC_CHECK(event_log_started);
  MultiChannelTest test;
  RunBaseTest(&test);
}

}  // namespace test
}  // namespace webrtc
