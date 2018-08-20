/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_SCENARIO_CONFIG_H_
#define TEST_SCENARIO_SCENARIO_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtpparameters.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/video_codec.h"
#include "test/frame_generator.h"

namespace webrtc {
namespace test {
struct PacketOverhead {
  static constexpr size_t kIpv4 = 20;
  static constexpr size_t kIpv6 = 40;
  static constexpr size_t kUdp = 8;
  static constexpr size_t kSrtp = 10;
  static constexpr size_t kTurn = 4;
  static constexpr size_t kDefault = kIpv4 + kUdp + kSrtp;
};

struct CallClientConfig {
  struct Rates {
    Rates();
    Rates(const Rates&);
    ~Rates();
    DataRate min_rate = DataRate::Zero();
    DataRate max_rate = DataRate::Infinity();
    DataRate start_rate = DataRate::kbps(300);
  } rates;
  struct CongestionControl {
    enum Type { kBbr, kGoogCc } type = kGoogCc;
    TimeDelta log_interval = TimeDelta::ms(100);
  } cc;
  TimeDelta stats_log_interval = TimeDelta::ms(100);
};

struct VideoStreamConfig {
  struct Source {
    enum Capture {
      kGenerator,
      kVideoFile,
      // kForward,
      // kImages
    } capture = Capture::kGenerator;
    struct Generator {
      using PixelFormat = FrameGenerator::OutputType;
      PixelFormat pixel_format = PixelFormat::I420;
    } generator;
    struct VideoFile {
      std::string name;
    } video_file;
    int width = 320;
    int height = 180;
    int framerate = 30;
  } source;
  struct Encoder {
    Encoder();
    Encoder(const Encoder&);
    ~Encoder();
    enum Implementation { kFake, kSoftware, kHardware } implementation = kFake;
    struct Fake {
      DataRate max_rate = DataRate::Infinity();
    } fake;

    using Codec = VideoCodecType;
    Codec codec = Codec::kVideoCodecGeneric;
    bool denoising = true;
    absl::optional<int> key_frame_interval = 3000;

    absl::optional<DataRate> max_data_rate;
    size_t num_simulcast_streams = 1;
    using DegradationPreference = DegradationPreference;
    DegradationPreference degradation_preference =
        DegradationPreference::MAINTAIN_FRAMERATE;
  } encoder;
  struct Stream {
    Stream();
    Stream(const Stream&);
    ~Stream();
    std::vector<uint32_t> ssrcs;
    std::vector<uint32_t> rtx_ssrcs;
    bool packet_feedback = true;
    TimeDelta nack_history_time = TimeDelta::Zero();
    size_t num_rtx_streams = 1;
    bool use_flexfec = false;
    bool use_ulpfec = false;
    DataSize packet_overhead = DataSize::bytes(PacketOverhead::kDefault);
  } stream;
  struct Renderer {
    enum Type { kFake } type = kFake;
  };
};

struct AudioStreamConfig {
  AudioStreamConfig();
  AudioStreamConfig(const AudioStreamConfig&);
  ~AudioStreamConfig();
  struct Source {
    int channels = 1;
  } source;
  struct Encoder {
    Encoder();
    Encoder(const Encoder&);
    ~Encoder();
    absl::optional<DataRate> target_rate;
    absl::optional<DataRate> min_rate;
    absl::optional<DataRate> max_rate;
    TimeDelta frame_length = TimeDelta::ms(20);
  } encoder;
  struct Stream {
    Stream();
    Stream(const Stream&);
    ~Stream();
    bool bitrate_tracking = false;
    absl::optional<uint32_t> ssrc;
    DataSize packet_overhead = DataSize::bytes(PacketOverhead::kDefault);
  } stream;
  struct Render {
    std::string sync_group;
  } render;
};

struct NetworkNodeConfig {
  NetworkNodeConfig();
  NetworkNodeConfig(const NetworkNodeConfig&);
  ~NetworkNodeConfig();
  enum TrafficMode { kSimulation, kCustom } mode = kSimulation;
  struct Simulation {
    Simulation();
    Simulation(const Simulation&);
    ~Simulation();
    DataRate bandwidth = DataRate::Infinity();
    TimeDelta delay = TimeDelta::Zero();
    TimeDelta delay_std_dev = TimeDelta::Zero();
    double loss_rate = 0;
  } simulation;
  DataSize packet_overhead = DataSize::Zero();
  TimeDelta update_frequency = TimeDelta::ms(1);
};

struct CrossTrafficConfig {
  CrossTrafficConfig();
  CrossTrafficConfig(const CrossTrafficConfig&);
  ~CrossTrafficConfig();
  enum Mode { kRandomWalk, kPwm } mode = kRandomWalk;
  int random_seed = 1;
  DataRate peak_rate = DataRate::kbps(100);
  DataSize min_packet_size = DataSize::bytes(200);
  TimeDelta min_packet_interval = TimeDelta::ms(1);
  struct RandomWalk {
    TimeDelta update_interval = TimeDelta::ms(200);
    double variance = 0.6;
    double bias = -0.1;
  } random_walk;
  struct Pwm {
    TimeDelta send_duration = TimeDelta::ms(100);
    TimeDelta hold_duration = TimeDelta::ms(2000);
  } pwm;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_SCENARIO_CONFIG_H_
