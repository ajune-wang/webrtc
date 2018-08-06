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

enum AudioBwe : int { kNone = 0, kFixed = 1, kDynamic = 2 };

struct CallTestConfig {
  int capacity_kbps = 150;
  int delay_ms = 100;
  int cross_traffic_kbps = 0;
  int normal_ms = 0;
  int interference_ms = 0;
  int loss_percent = 0;
  AudioBwe audio_bwe = kNone;
  int random_seed = 0;

  std::string AdditionalTrials() const {
    if (audio_bwe) {
      return "/WebRTC-Audio-SendSideBwe/Enabled"
             "/WebRTC-SendSideBwe-WithOverhead/Enabled";
    }
    return "";
  }
  bool delay_mode() const { return cross_traffic_kbps == capacity_kbps; }
  TimeDelta interference_interval() const {
    return TimeDelta::ms(interference_ms + normal_ms);
  }
  TimeDelta interference_duration() const {
    return TimeDelta::ms(interference_ms);
  }
  std::string Name() const {
    char raw_name[256];
    rtc::SimpleStringBuilder name(raw_name);

    name.AppendFormat("_ab%i", audio_bwe);
    name.AppendFormat("_dl%i", delay_ms);
    name.AppendFormat("_bw%i", capacity_kbps);
    name.AppendFormat("_ct%i_cl%i_ch%i", cross_traffic_kbps, normal_ms,
                      interference_ms);
    name.AppendFormat("_lr%i", loss_percent);
    name.AppendFormat("_rs%i", random_seed);
    return name.str();
  }
};
}  // namespace
class GoogCcScenarioTest : public ::testing::Test,
                           public testing::WithParamInterface<
                               tuple<int, int, int, int, int, int, int, int>> {
 public:
  GoogCcScenarioTest() {
    conf_.random_seed = ::testing::get<0>(GetParam());
    conf_.delay_ms = ::testing::get<1>(GetParam());
    conf_.capacity_kbps = ::testing::get<2>(GetParam());
    conf_.cross_traffic_kbps = ::testing::get<3>(GetParam());
    conf_.normal_ms = ::testing::get<4>(GetParam());
    conf_.interference_ms = ::testing::get<5>(GetParam());
    conf_.loss_percent = ::testing::get<6>(GetParam());
    conf_.audio_bwe = (AudioBwe)::testing::get<7>(GetParam());
    field_trial_.reset(new test::ScopedFieldTrials(
        "WebRTC-TaskQueueCongestionControl/Enabled" + conf_.AdditionalTrials() +
        "/"));
  }
  CallTestConfig conf_;

 private:
  std::unique_ptr<test::ScopedFieldTrials> field_trial_;
};

TEST_P(GoogCcScenarioTest, ReceivesVideo) {
  std::string base_name = "/datadump/googcc_test_gen/scen_" + conf_.Name();
  Scenario s(base_name);
  CallClient* alice = s.CreateClient("send", [&](CallClientConfig* c) {});
  CallClient* bob = s.CreateClient("return", [&](CallClientConfig* c) {});
  FakeNetworkPipe::Config net_conf;
  net_conf.link_capacity_kbps = conf_.capacity_kbps;
  net_conf.queue_delay_ms = conf_.delay_ms;
  net_conf.loss_percent = conf_.loss_percent;
  SimulationNode* send_net = s.CreateNetworkNode(net_conf);
  SimulationNode* ret_net = s.CreateNetworkNode(net_conf);
  auto video_send = s.CreateVideoStreams(
      alice, {send_net}, bob, {ret_net}, [&](VideoStreamConfig* c) {
        c->encoder.max_data_rate = DataRate::kbps(2000);
      });
  s.CreateAudioStreams(alice, {send_net}, bob, {ret_net},
                       [&](AudioStreamConfig* c) {
                         if (conf_.audio_bwe) {
                           c->stream.bitrate_tracking = true;
                           if (conf_.audio_bwe == kFixed) {
                             c->encoder.target_rate = DataRate::kbps(31);
                           } else if (conf_.audio_bwe == kDynamic) {
                             c->encoder.min_rate = DataRate::kbps(8);
                             c->encoder.max_rate = DataRate::kbps(31);
                           }
                         }
                       });
  CrossTrafficSource* cross_traffic =
      s.CreateCrossTraffic({send_net}, [&](CrossTrafficConfig* c) {
        c->mode = CrossTrafficConfig::Mode::kPwm;
        c->peak_rate = DataRate::kbps(conf_.cross_traffic_kbps);
        if (conf_.delay_mode())
          c->peak_rate = DataRate::Zero();
        c->pwm.hold_duration = TimeDelta::ms(conf_.normal_ms);
        c->pwm.send_duration = TimeDelta::ms(conf_.interference_ms);
      });
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

  ColumnPrinter delay_printer(
      base_name + ".send.delay.txt",
      {s.TimePrinter(),
       LambdaPrinter("duration", [&](rtc::SimpleStringBuilder& sb) {
         sb.AppendFormat("%.3lf", conf_.interference_ms / 1000.0);
       })});
  delay_printer.PrintHeaders();
  if (conf_.delay_mode()) {
    s.Every(conf_.interference_interval(), [&]() {
      delay_printer.PrintRow();
      send_net->TriggerDelay(conf_.interference_duration());
    });
  }
  s.RunFor(TimeDelta::ms(kRunTimeMs));
}

INSTANTIATE_TEST_CASE_P(
    HighBwDelay,
    GoogCcScenarioTest,
    Values(make_tuple(1, 50, 1000, 1000, 2500, 50, 0, kNone),
           make_tuple(1, 50, 2000, 2000, 2500, 50, 0, kNone)));
INSTANTIATE_TEST_CASE_P(LowBwClean,
                        GoogCcScenarioTest,
                        Values(make_tuple(1, 50, 70, 0, 0, 0, 0, kNone),
                               make_tuple(1, 50, 100, 0, 0, 0, 0, kNone),
                               make_tuple(1, 50, 120, 0, 0, 0, 0, kNone),
                               make_tuple(1, 50, 70, 0, 0, 0, 0, kFixed),
                               make_tuple(1, 50, 70, 0, 0, 0, 0, kDynamic)));

}  // namespace test
}  // namespace webrtc
