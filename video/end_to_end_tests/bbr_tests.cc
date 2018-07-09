/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "modules/congestion_controller/bbr/test/bbr_printer.h"
#include "modules/congestion_controller/goog_cc/test/goog_cc_printer.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "video/end_to_end_tests/congestion_controller_test.h"

namespace webrtc {
namespace {

class GroundTruthPrinter {
 public:
  explicit GroundTruthPrinter(std::string filename) {
    output_file_ = fopen(filename.c_str(), "w");
    RTC_CHECK(output_file_);
    output_ = output_file_;
  }
  GroundTruthPrinter() { output_ = stdout; }
  ~GroundTruthPrinter() {
    if (output_file_)
      fclose(output_file_);
  }
  void PrintHeaders() {
    fprintf(output_, "time propagation_delay capacity cross_traffic\n");
  }
  void PrintStats(int64_t time_ms,
                  int64_t propagation_delay_ms,
                  int64_t capacity_kbps,
                  int64_t cross_traffic_bps) {
    fprintf(output_, "%.3lf %.3lf %.0lf %.0lf\n", time_ms / 1000.0,
            propagation_delay_ms / 1000.0, capacity_kbps * 1000 / 8.0,
            cross_traffic_bps / 8.0);
  }
  FILE* output_file_ = nullptr;
  FILE* output_ = nullptr;
};

constexpr int64_t kRunTimeMs = 60000;

using ::testing::Values;
using ::testing::Combine;
using ::testing::tuple;
using ::testing::make_tuple;
enum CcImpl : int { kNone = 0, kGcc = 1, kBbr = 2 };
enum AudioMode : int { kAudioOff = 0, kAudioOn = 1, kAudioBwe = 2 };
enum BbrTuning : int {
  kBbrTuningOff = 0,
  kBbrTargetRate = 1,
  kBbrInitialWindow = 2,
  kBbrBoth = 3
};
#define NO_CC 0
#define GOOG_CC 1
#define BBR_CC 2

struct CallTestConfig {
  CcImpl send = kGcc;
  CcImpl ret = kNone;
  AudioMode audio_mode = kAudioOff;
  int capacity_kbps = 150;
  int delay_ms = 100;
  double encoder_gain = 1;
  BbrTuning bbr_tuning = kBbrTuningOff;
  bool pacing_target() const { return !(bbr_tuning & kBbrTargetRate); }
  bool initial_window() const { return bbr_tuning & kBbrInitialWindow; }
  int cross_traffic_seed = 0;
  int delay_noise_ms = 0;
  int loss_percent = 0;

  std::string BbrTrial() const {
    char trial_buf[1024];
    rtc::SimpleStringBuilder trial(trial_buf);
    trial << "WebRTC-BweBbrConfig/";
    trial << "encoder_rate_gain_in_probe_rtt:0.5";
    trial.AppendFormat(",encoder_rate_gain:%.1lf", encoder_gain);
    if (pacing_target())
      trial << ",pacing_rate_as_target:1";
    if (initial_window())
      trial << ",initial_cwin:8000";
    return trial.str();
  }
  std::string AdditionalTrials() const {
    if (audio_mode == kAudioBwe) {
      return "/WebRTC-Audio-SendSideBwe/Enabled"
             "/WebRTC-SendSideBwe-WithOverhead/Enabled";
    }
    return "";
  }
  std::string Name() const {
    char bbr_name_buf[128];
    rtc::SimpleStringBuilder bbr_name(bbr_name_buf);
    bbr_name << "bbr";

    if (pacing_target())
      bbr_name << "-pt";
    if (initial_window())
      bbr_name << "-iw";
    bbr_name.AppendFormat("-eg%.0lf", encoder_gain * 100);
    char raw_name[256];
    rtc::SimpleStringBuilder name(raw_name);
    name.AppendFormat("_%ikbps_%ims_a%i_", capacity_kbps, delay_ms, audio_mode);
    if (delay_noise_ms > 0)
      name.AppendFormat("dn%i_", delay_noise_ms);
    if (loss_percent > 0)
      name.AppendFormat("lr%i_", loss_percent);
    if (cross_traffic_seed > 0)
      name.AppendFormat("ct%i_", cross_traffic_seed);
    if (send == kBbr)
      name << bbr_name.str();
    else
      name << "googcc";

    if (ret == kGcc) {
      name << "_googcc";
    } else if (ret == kBbr) {
      name << "_bbr";
    } else {
      name << "_none";
    }
    return name.str();
  }
};

class BbrTestObserver : public test::BaseCongestionControllerTest {
 public:
  explicit BbrTestObserver(CallTestConfig conf)
      : BaseCongestionControllerTest(
            kRunTimeMs,
            "/datadump/endtoend_test_gen/bbr_" + conf.Name()),
        cross_random_(std::max(conf.cross_traffic_seed, 1)),
        conf_(conf),
        send_truth_printer_(filepath_base_ + "_send.truth.txt"),
        recv_truth_printer_(filepath_base_ + "_recv.truth.txt") {
    config_.link_capacity_kbps = conf_.capacity_kbps;
    config_.queue_delay_ms = conf_.delay_ms;
    config_.delay_standard_deviation_ms = conf_.delay_noise_ms;
    config_.allow_reordering = false;
    config_.loss_percent = conf_.loss_percent;
    //      config_.queue_length_packets = 60;
    send_truth_printer_.PrintHeaders();
    recv_truth_printer_.PrintHeaders();
  }

  ~BbrTestObserver() {}

 private:
  size_t GetNumVideoStreams() const override { return 1; }
  size_t GetNumAudioStreams() const override {
    return (conf_.audio_mode != kAudioOff) ? 1 : 0;
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    BaseCongestionControllerTest::OnCallsCreated(sender_call, receiver_call);
    BitrateSettings settings;
    settings.max_bitrate_bps = 1800000;
    settings.start_bitrate_bps = 300000;
    settings.min_bitrate_bps = 30000;
    sender_call->GetTransportControllerSend()->SetClientBitratePreferences(
        settings);
    receiver_call->GetTransportControllerSend()->SetClientBitratePreferences(
        settings);
  }
  test::PacketTransport* CreateSendTransport(
      test::SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override {
    auto send_pipe = absl::make_unique<webrtc::FakeNetworkPipe>(
        Clock::GetRealTimeClock(), config_);
    send_pipe_ = send_pipe.get();
    send_transport_ = new test::PacketTransport(
        task_queue, sender_call, this, test::PacketTransport::kSender,
        test::CallTest::payload_type_map_, std::move(send_pipe));
    return send_transport_;
  }

  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override {
    // send_config->send_codec_spec->target_bitrate_bps = 32000;
    // send_config->min_bitrate_bps = 32000;
    // send_config->max_bitrate_bps = 32000;
    send_config->send_codec_spec->transport_cc_enabled = true;

    send_config->rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri, 8));
    for (AudioReceiveStream::Config& recv_config : *receive_configs) {
      recv_config.rtp.transport_cc = true;
      recv_config.rtp.extensions = send_config->rtp.extensions;
      recv_config.rtp.remote_ssrc = send_config->rtp.ssrc;
    }
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    encoder_config->max_bitrate_bps = 2000000;
  }

  void PerformTest() override {
    Clock* clock = Clock::GetRealTimeClock();
    int64_t first_update_ms = clock->TimeInMilliseconds();
    int64_t last_state_update_ms = 0;
    do {
      int64_t now_ms = clock->TimeInMilliseconds();
      if (now_ms - first_update_ms > kRunTimeMs)
        break;

      if (now_ms - last_state_update_ms > 100) {
        last_state_update_ms = now_ms;
        PrintStates(now_ms);
        PrintStats(now_ms);
        send_truth_printer_.PrintStats(now_ms, config_.queue_delay_ms,
                                       config_.link_capacity_kbps, 0);
        recv_truth_printer_.PrintStats(now_ms, config_.queue_delay_ms,
                                       config_.link_capacity_kbps, 0);
      }
    } while (!observation_complete_.Wait(5));
  }

  std::pair<std::unique_ptr<NetworkControllerFactoryInterface>,
            std::unique_ptr<DebugStatePrinter>>
  CreateSendCCFactory(RtcEventLog* event_log) override {
    if (conf_.ret == kBbr) {
      auto bbr_printer = absl::make_unique<BbrStatePrinter>();
      auto return_factory =
          absl::make_unique<BbrDebugFactory>(bbr_printer.get());
      return {std::move(return_factory), std::move(bbr_printer)};
    } else {
      auto goog_printer = absl::make_unique<GoogCcStatePrinter>();
      auto return_factory =
          absl::make_unique<GoogCcDebugFactory>(event_log, goog_printer.get());
      return {std::move(return_factory), std::move(goog_printer)};
    }
  }

  webrtc::Random cross_random_;
  FakeNetworkPipe::Config config_;
  const CallTestConfig conf_;
  GroundTruthPrinter send_truth_printer_;
  GroundTruthPrinter recv_truth_printer_;
  test::PacketTransport* send_transport_ = nullptr;
  FakeNetworkPipe* send_pipe_ = nullptr;
};
}  // namespace
class BbrEndToEndTest
    : public test::CallTest,
      public testing::WithParamInterface<
          tuple<CcImpl, CcImpl, int, int, int, double, int, int, int, int>> {
 public:
  BbrEndToEndTest() {
    conf_.send = (CcImpl)::testing::get<0>(GetParam());
    conf_.ret = (CcImpl)::testing::get<1>(GetParam());
    conf_.audio_mode = (AudioMode)::testing::get<2>(GetParam());
    conf_.capacity_kbps = ::testing::get<3>(GetParam());
    conf_.delay_ms = ::testing::get<4>(GetParam());
    conf_.encoder_gain = ::testing::get<5>(GetParam());
    conf_.bbr_tuning = (BbrTuning)::testing::get<6>(GetParam());
    conf_.cross_traffic_seed = ::testing::get<7>(GetParam());
    conf_.loss_percent = ::testing::get<8>(GetParam());
    conf_.delay_noise_ms = ::testing::get<9>(GetParam());

    field_trial_.reset(new test::ScopedFieldTrials(
        "WebRTC-TaskQueueCongestionControl/Enabled"
        "/WebRTC-PacerPushbackExperiment/Enabled"
        "/WebRTC-Pacer-DrainQueue/Disabled"
        "/WebRTC-Pacer-PadInSilence/Enabled"
        "/WebRTC-Pacer-BlockAudio/Disabled" +
        conf_.AdditionalTrials() +
        "/WebRTC-BweCongestionController/Enabled,BBR/" + conf_.BbrTrial() +
        "/"));
  }

  CallTestConfig conf_;

 private:
  std::unique_ptr<test::ScopedFieldTrials> field_trial_;
};

TEST_P(BbrEndToEndTest, SendTraffic) {
  BbrTestObserver test(conf_);
  RunBaseTest(&test);
}

// audio_mode, capacity_kbps, delay_ms,
// short_startup, initial_window, minimum_window
// audio_bwe
INSTANTIATE_TEST_CASE_P(
    BbrOneWay,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kNone, 2, 100, 100, 1, 3, 0, 0, 0),

           make_tuple(kBbr, kNone, 2, 150, 100, 1., 1, 0, 0, 0),

           make_tuple(kBbr, kNone, 2, 150, 100, 1., 1, 0, 0, 0),

           make_tuple(kBbr, kNone, 2, 150, 100, 1., 3, 0, 0, 0),
           make_tuple(kBbr, kNone, 2, 150, 100, 1., 2, 0, 0, 0),

           make_tuple(kBbr, kNone, 2, 150, 100, .90, 3, 0, 0, 0),
           make_tuple(kBbr, kNone, 2, 150, 100, .80, 3, 0, 0, 0),

           make_tuple(kBbr, kNone, 2, 800, 100, 1., 3, 0, 0, 0),

           make_tuple(kBbr, kNone, 2, 800, 100, .8, 3, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    BbrTwoWayTunings,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kBbr, 2, 150, 100, 1, 1, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    GoogCcOneWay,
    BbrEndToEndTest,
    Values(make_tuple(kGcc, kNone, 1, 150, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kNone, 1, 800, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kNone, 1, 800, 50, 1, 1, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    CrossTraffic,
    BbrEndToEndTest,
    Values(make_tuple(kGcc, kNone, 1, 800, 100, 0.0, 0, 1, 0, 0),
           make_tuple(kBbr, kNone, 2, 800, 100, 0.8, 3, 1, 0, 0),
           make_tuple(kGcc, kNone, 1, 800, 100, 0.0, 0, 2, 0, 0),
           make_tuple(kBbr, kNone, 2, 800, 100, 0.8, 3, 2, 0, 0),
           make_tuple(kGcc, kNone, 1, 150, 100, 0.0, 0, 1, 0, 0),
           make_tuple(kBbr, kNone, 2, 150, 100, 0.8, 3, 1, 0, 0),
           make_tuple(kGcc, kNone, 1, 150, 100, 0.0, 0, 2, 0, 0),
           make_tuple(kBbr, kNone, 2, 150, 100, 0.8, 3, 2, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    NetworkDegradations,
    BbrEndToEndTest,
    Values(make_tuple(kGcc, kNone, 1, 800, 100, 1, 1, 0, 5, 30),
           make_tuple(kBbr, kNone, 2, 800, 100, 0.8, 3, 0, 5, 30),
           make_tuple(kGcc, kNone, 1, 150, 100, 1, 1, 0, 5, 30),
           make_tuple(kBbr, kNone, 2, 150, 100, 0.8, 3, 0, 5, 30)));

INSTANTIATE_TEST_CASE_P(
    BbrVsGoogCc,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kGcc, 2, 150, 100, .8, 3, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    BbrNoAudioBwe,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kBbr, 1, 150, 100, .8, 3, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    GoogCcAudioBwe,
    BbrEndToEndTest,
    Values(make_tuple(kGcc, kNone, 2, 150, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 2, 150, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 2, 100, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 2, 100, 50, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 2, 800, 100, 1, 1, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    BbrTwoWayVariations,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kBbr, 2, 100, 50, .8, 3, 0, 0, 0),
           make_tuple(kBbr, kBbr, 2, 150, 50, .8, 3, 0, 0, 0),
           make_tuple(kBbr, kBbr, 2, 800, 50, .8, 3, 0, 0, 0),
           make_tuple(kBbr, kBbr, 2, 100, 100, .8, 3, 0, 0, 0),
           make_tuple(kBbr, kBbr, 2, 150, 100, .8, 3, 0, 0, 0),
           make_tuple(kBbr, kBbr, 2, 800, 100, .8, 3, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    GoogCcTwoWayVariations,
    BbrEndToEndTest,
    Values(make_tuple(kGcc, kGcc, 1, 100, 50, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 1, 150, 50, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 1, 800, 50, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 1, 100, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 1, 150, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 1, 800, 100, 1, 1, 0, 0, 0)));
/*
INSTANTIATE_TEST_CASE_P(
    GoogCc70Kbps,
    BbrEndToEndTest,
    Values(make_tuple(kGcc, kGcc, 0, 70, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kNone, 0, 70, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 1, 70, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kGcc, 2, 70, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kNone, 1, 70, 100, 1, 1, 0, 0, 0),
           make_tuple(kGcc, kNone, 2, 70, 100, 1, 1, 0, 0, 0)));*/
}  // namespace webrtc
