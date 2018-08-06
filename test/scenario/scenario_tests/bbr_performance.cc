/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/random.h"

#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/scenario/scenario.h"

namespace webrtc {
namespace test {
namespace {
constexpr int64_t kRunTimeMs = 60000;

using ::testing::Values;
using ::testing::Combine;
using ::testing::tuple;
using ::testing::make_tuple;
enum CcImpl : int { kNone = 0, kGcc = 1, kBbr = 2 };
enum BbrTuning : int {
  kOff = 0,
  kTarget = 1,  // Separate target rate
  kWindow = 2,  // Decrease initial window
  kBoth = 3     // Both tunings
};

CallClientConfig::CongestionControl::Type GetCC(CcImpl cc_impl) {
  if (cc_impl == kBbr)
    return CallClientConfig::CongestionControl::Type::kBbr;
  return CallClientConfig::CongestionControl::Type::kGoogCc;
}

struct CallTestConfig {
  CcImpl send = kGcc;
  CcImpl ret = kNone;
  int capacity_kbps = 150;
  int delay_ms = 100;
  int encoder_gain_percent = 100;
  BbrTuning bbr_tuning = kOff;
  bool custom_target() const { return bbr_tuning & kTarget; }
  bool initial_window() const { return bbr_tuning & kWindow; }
  bool audio = false;
  int random_seed = 0;
  int cross_traffic_kbps = 0;
  int delay_noise_ms = 0;
  int loss_percent = 0;

  std::string BbrTrial() const {
    char trial_buf[1024];
    rtc::SimpleStringBuilder trial(trial_buf);
    trial << "WebRTC-BweBbrConfig/";
    trial << "encoder_rate_gain_in_probe_rtt:0.5";
    trial.AppendFormat(",encoder_rate_gain:%.1lf",
                       encoder_gain_percent / 100.0);
    if (!custom_target())
      trial << ",pacing_rate_as_target:1";
    if (initial_window())
      trial << ",initial_cwin:8000";
    return trial.str();
  }
  std::string AdditionalTrials() const {
    if (send == kGcc) {
      return "/WebRTC-PacerPushbackExperiment/Enabled"
             "/WebRTC-Pacer-DrainQueue/Disabled"
             "/WebRTC-Pacer-PadInSilence/Enabled"
             "/WebRTC-Pacer-BlockAudio/Disabled"
             "/WebRTC-Audio-SendSideBwe/Enabled"
             "/WebRTC-SendSideBwe-WithOverhead/Enabled";
    }
    return "";
  }
  std::string Name() const {
    char raw_name[256];
    rtc::SimpleStringBuilder name(raw_name);

    name.AppendFormat("_au%i", audio);
    name.AppendFormat("_bw%i_ct%i", capacity_kbps, cross_traffic_kbps);
    name.AppendFormat("_dl%i_dn%i", delay_ms, delay_noise_ms);
    name.AppendFormat("_lr%i", loss_percent);

    if (send == kBbr) {
      char bbr_name_buf[128];
      rtc::SimpleStringBuilder bbr_name(bbr_name_buf);
      bbr_name << "_bbr";
      if (custom_target())
        bbr_name << "-tg";
      if (initial_window())
        bbr_name << "-iw";
      bbr_name.AppendFormat("-eg%i", encoder_gain_percent);
      name << bbr_name.str();
    } else {
      name << "_googcc";
    }

    if (ret == kGcc) {
      name << "_googcc";
    } else if (ret == kBbr) {
      name << "_bbr";
    } else {
      name << "_none";
    }
    name.AppendFormat("_rs%i", random_seed);
    return name.str();
  }
};
}  // namespace
class BbrEndToEndTest
    : public ::testing::Test,
      public testing::WithParamInterface<
          tuple<CcImpl, CcImpl, int, int, int, int, int, int, int, int, int>> {
 public:
  BbrEndToEndTest() {
    conf_.send = (CcImpl)::testing::get<0>(GetParam());
    conf_.ret = (CcImpl)::testing::get<1>(GetParam());

    conf_.audio = ::testing::get<2>(GetParam());
    conf_.random_seed = ::testing::get<3>(GetParam());
    conf_.capacity_kbps = ::testing::get<4>(GetParam());
    conf_.cross_traffic_kbps = ::testing::get<5>(GetParam());
    conf_.delay_ms = ::testing::get<6>(GetParam());
    conf_.delay_noise_ms = ::testing::get<7>(GetParam());
    conf_.loss_percent = ::testing::get<8>(GetParam());

    conf_.bbr_tuning = (BbrTuning)::testing::get<9>(GetParam());
    conf_.encoder_gain_percent = ::testing::get<10>(GetParam());

    field_trial_.reset(new test::ScopedFieldTrials(
        "WebRTC-TaskQueueCongestionControl/Enabled" + conf_.AdditionalTrials() +
        "/WebRTC-BweCongestionController/Enabled,BBR/" + conf_.BbrTrial() +
        "/"));
  }

  CallTestConfig conf_;

 private:
  std::unique_ptr<test::ScopedFieldTrials> field_trial_;
};

TEST_P(BbrEndToEndTest, ReceivesVideo) {
  std::string base_name = "/datadump/scenario_test_gen/bbr_" + conf_.Name();
  RTC_LOG(LS_INFO) << "Saving log to: " << base_name;

  Scenario s(base_name);
  CallClientConfig::Rates rate_config;
  rate_config.min_rate = DataRate::kbps(30);
  rate_config.max_rate = DataRate::kbps(1800);
  rate_config.start_rate = DataRate::kbps(300);

  CallClient* alice = s.CreateClient("send", [&](CallClientConfig* c) {
    c->cc.type = GetCC(conf_.send);
    c->cc.log_interval = TimeDelta::ms(100);
    c->rates = rate_config;
  });
  CallClient* bob = s.CreateClient("return", [&](CallClientConfig* c) {
    c->cc.type = GetCC(conf_.ret);
    c->cc.log_interval = TimeDelta::ms(100);
    c->rates = rate_config;
  });

  SimulationNode* send_net = s.CreateNetworkNode();
  SimulationNode* ret_net = s.CreateNetworkNode();
  FakeNetworkPipe::Config net_conf;
  net_conf.link_capacity_kbps = conf_.capacity_kbps;
  net_conf.queue_delay_ms = conf_.delay_ms;
  net_conf.delay_standard_deviation_ms = conf_.delay_noise_ms;
  net_conf.allow_reordering = false;
  net_conf.loss_percent = conf_.loss_percent;
  send_net->SetConfig(net_conf);
  ret_net->SetConfig(net_conf);
  auto video_send = s.CreateVideoStreams(
      alice, {send_net}, bob, {ret_net}, [&](VideoStreamConfig* c) {
        c->encoder.max_data_rate = DataRate::kbps(2000);
      });
  if (conf_.audio) {
    s.CreateAudioStreams(alice, {send_net}, bob, {ret_net},
                         [&](AudioStreamConfig* c) {
                           if (conf_.send == kBbr) {
                             c->stream.bitrate_tracking = true;
                             c->encoder.target_rate = DataRate::kbps(31);
                           }
                         });
  }

  if (conf_.ret != kNone) {
    s.CreateVideoStreams(bob, {ret_net}, alice, {send_net},
                         [&](VideoStreamConfig* c) {
                           c->encoder.max_data_rate = DataRate::kbps(2000);
                         });
  }
  CrossTrafficConfig cross_config;
  cross_config.peak_rate = DataRate::kbps(conf_.cross_traffic_kbps);
  cross_config.random_seed = conf_.random_seed + 100;
  CrossTrafficSource* cross_traffic =
      s.CreateCrossTraffic({send_net}, cross_config);

  ColumnPrinter send_stats_printer(
      base_name + ".send.stats.txt",
      {s.TimePrinter(), alice->StatsPrinter(), video_send.first->StatsPrinter(),
       cross_traffic->StatsPrinter(),
       LambdaPrinter(
           "propagation_delay capacity", [&](rtc::SimpleStringBuilder& sb) {
             sb.AppendFormat("%.3lf %.0lf", net_conf.queue_delay_ms / 1000.0,
                             net_conf.link_capacity_kbps * 1000 / 8.0);
           })});
  send_stats_printer.PrintHeaders();
  s.Every(TimeDelta::ms(100), [&]() { send_stats_printer.PrintRow(); });

  s.RunFor(TimeDelta::ms(kRunTimeMs));
}

INSTANTIATE_TEST_CASE_P(
    OneWayTuning,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kNone, 1, 1, 150, 0, 100, 0, 0, kOff, 100),
           make_tuple(kBbr, kNone, 1, 1, 150, 0, 100, 0, 0, kWindow, 100),
           make_tuple(kBbr, kNone, 1, 1, 150, 0, 100, 0, 0, kBoth, 100),
           make_tuple(kBbr, kNone, 1, 1, 150, 0, 100, 0, 0, kBoth, 80)));

INSTANTIATE_TEST_CASE_P(
    OneWayTuned,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kNone, 1, 1, 150, 0, 100, 0, 0, kBoth, 80),
           make_tuple(kGcc, kNone, 1, 1, 150, 0, 100, 0, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    OneWayDegraded,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kNone, 1, 1, 150, 0, 100, 30, 5, kBoth, 80),
           make_tuple(kGcc, kNone, 1, 1, 150, 0, 100, 30, 5, 0, 0),

           make_tuple(kBbr, kNone, 1, 1, 150, 100, 100, 0, 0, kBoth, 80),
           make_tuple(kGcc, kNone, 1, 1, 150, 100, 100, 0, 0, 0, 0),

           make_tuple(kBbr, kNone, 1, 1, 800, 0, 100, 30, 5, kBoth, 80),
           make_tuple(kGcc, kNone, 1, 1, 800, 0, 100, 30, 5, 0, 0),

           make_tuple(kBbr, kNone, 1, 1, 800, 600, 100, 0, 0, kBoth, 80),
           make_tuple(kGcc, kNone, 1, 1, 800, 600, 100, 0, 0, 0, 0)));

INSTANTIATE_TEST_CASE_P(
    TwoWay,
    BbrEndToEndTest,
    Values(make_tuple(kBbr, kBbr, 1, 1, 150, 0, 100, 0, 0, kBoth, 80),
           make_tuple(kGcc, kGcc, 1, 1, 150, 0, 100, 0, 0, 0, 0),
           make_tuple(kBbr, kBbr, 1, 1, 800, 0, 100, 0, 0, kBoth, 80),
           make_tuple(kGcc, kGcc, 1, 1, 800, 0, 100, 0, 0, 0, 0),
           make_tuple(kBbr, kBbr, 1, 1, 150, 0, 50, 0, 0, kBoth, 80),
           make_tuple(kGcc, kGcc, 1, 1, 150, 0, 50, 0, 0, 0, 0)));
}  // namespace test
}  // namespace webrtc
