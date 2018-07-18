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

CallClientConfig::CongestionControl::Type GetCC(CcImpl cc_impl) {
  if (cc_impl == kBbr)
    return CallClientConfig::CongestionControl::Type::kBbr;
  return CallClientConfig::CongestionControl::Type::kGoogCc;
}

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
}  // namespace
class BbrEndToEndTest
    : public ::testing::Test,
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

template <typename STRUCT_T>
STRUCT_T Set(std::function<void(STRUCT_T&)> setter) {
  STRUCT_T local_struct;
  setter(local_struct);
  return local_struct;
}
CallClientConfig Set(std::function<void(CallClientConfig&)> setter);
VideoStreamConfig Set(std::function<void(VideoStreamConfig&)> setter);
AudioStreamConfig Set(std::function<void(AudioStreamConfig&)> setter);

class CrossTrafficSource {
 public:
  CrossTrafficSource(NetworkNode* send_net, DataRate capacity, int random_seed)
      : send_net_(send_net),
        capacity_(capacity),
        random_(std::max(random_seed, 1)) {}
  void Update(TimeDelta delta) {
    const double kCrossVariationPerSec = 0.6;
    const double kCrossVariationBias = -0.1;
    intensity_ += random_.Gaussian(kCrossVariationBias, kCrossVariationPerSec) *
                  delta.seconds<double>();
    intensity_ = std::min(0.7, intensity_);
    intensity_ = std::max(0.0, intensity_);
  }
  DataRate Traffic() const { return capacity_ * intensity_; }
  void Process(TimeDelta delta) {
    pending_cross_packet_size += Traffic() * delta;
    if (pending_cross_packet_size > DataSize::bytes(200)) {
      send_net_->EnqueueCrossPacket(pending_cross_packet_size.bytes());
      pending_cross_packet_size = DataSize::Zero();
    }
  }

 private:
  NetworkNode* send_net_;
  DataRate capacity_;
  webrtc::Random random_;

  double intensity_ = 0;
  DataSize pending_cross_packet_size = DataSize::Zero();
};

TEST_F(BbrEndToEndTest, ReceivesVideo) {
  std::string base_name = "/datadump/endtoend_test_gen/bbr_" + conf_.Name();
  RTC_LOG(LS_INFO) << "Saving log to: " << base_name;

  Scenario s(base_name);
  CallClientConfig::Rates rate_config;
  rate_config.min_rate = DataRate::kbps(30);
  rate_config.max_rate = DataRate::kbps(1800);
  rate_config.start_rate = DataRate::kbps(300);

  CallClient* alice = s.CreateClient("send", Set([&](CallClientConfig c) {
                                       c.cc.type = GetCC(conf_.send);
                                       c.rates = rate_config;
                                     }));
  CallClient* bob = s.CreateClient("return", Set([&](CallClientConfig c) {
                                     c.cc.type = GetCC(conf_.ret);
                                     c.rates = rate_config;
                                   }));
  NetworkNode* send_net = s.CreateNetworkNode();
  NetworkNode* ret_net = s.CreateNetworkNode();
  FakeNetworkPipe::Config net_conf;
  net_conf.link_capacity_kbps = conf_.capacity_kbps;
  net_conf.queue_delay_ms = conf_.delay_ms;
  net_conf.delay_standard_deviation_ms = conf_.delay_noise_ms;
  net_conf.allow_reordering = false;
  net_conf.loss_percent = conf_.loss_percent;
  send_net->SetConfig(net_conf);
  ret_net->SetConfig(net_conf);
  auto video_send = s.CreateVideoStreams(
      alice, send_net, bob, ret_net, Set([&](VideoStreamConfig& c) {
        c.encoder.max_data_rate = DataRate::kbps(2000);
      }));
  if (conf_.audio_mode != kAudioOff) {
    s.CreateAudioStreams(alice, send_net, bob, ret_net,
                         Set([&](AudioStreamConfig& c) {
                           c.stream.bitrate_tracking = true;
                           c.encoder.fixed_rate = DataRate::kbps(31);
                         }));
  }
  if (conf_.ret != kNone) {
    s.CreateVideoStreams(bob, ret_net, alice, send_net,
                         Set([&](VideoStreamConfig& c) {
                           c.encoder.max_data_rate = DataRate::kbps(2000);
                         }));
    s.Every(TimeDelta::ms(100), [&]() { bob->LogCongestionControllerStats(); });
  }
  CrossTrafficSource cross_traffic(
      send_net, DataRate::kbps(conf_.capacity_kbps), conf_.cross_traffic_seed);
  ColumnPrinter send_stats_printer(
      base_name + "_send.stats.txt",
      {s.TimePrinter(),
       alice->StatsPrinter(),
       video_send.first->StatsPrinter(),
       {"propagation_delay capacity cross_traffic",
        [&](rtc::SimpleStringBuilder& sb) {
          sb.AppendFormat("%.3lf %.0lf %.0lf", net_conf.queue_delay_ms / 1000.0,
                          net_conf.link_capacity_kbps * 1000 / 8.0,
                          cross_traffic.Traffic().bps() / 8.0);
        }}});
  send_stats_printer.PrintHeaders();

  if (conf_.cross_traffic_seed) {
    s.Every(TimeDelta::ms(200),
            [&](TimeDelta delta) { cross_traffic.Update(delta); });
    s.Every(TimeDelta::ms(1),
            [&](TimeDelta delta) { cross_traffic.Process(delta); });
  }
  s.Every(TimeDelta::ms(100), [&]() { send_stats_printer.PrintRow(); });
  s.RunFor(TimeDelta::ms(kRunTimeMs));
}

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
}  // namespace test
}  // namespace webrtc
