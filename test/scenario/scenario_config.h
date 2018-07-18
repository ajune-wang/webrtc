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
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"

namespace webrtc {
namespace test {

struct CallClientConfig {
  struct Rates {
    absl::optional<DataRate> min_rate;
    absl::optional<DataRate> max_rate;
    absl::optional<DataRate> start_rate;
  } rates;
  struct CongestionControl {
    enum Type { kBbr, kGoogCc } type = kGoogCc;
  } cc;
};

struct VideoStreamConfig {
  struct Source {
    enum Capture {
      kNone,  // Using fake encoder
      kGenerate,
      kForward,
      kVideoFile,
      kImages
    } capture = kNone;
  } source;
  struct Encoder {
    enum Codec { kFake, kVp8, kVp9 } codec = kFake;
    absl::optional<DataRate> max_data_rate;
    struct Fake {
      DataRate max_rate = DataRate::Infinity();
    } fake;
    size_t num_simulcast_streams = 1;
  } encoder;
  struct Stream {
    std::vector<uint32_t> ssrcs;
    std::vector<uint32_t> rtx_ssrcs;
    bool packet_feedback = true;
    TimeDelta nack_history_time = TimeDelta::Zero();
    size_t num_rtx_streams = 1;
    bool use_flexfec = false;
    bool use_ulpfec = false;
  } stream;
  struct Renderer {
    enum Type { kFake } type = kFake;
  };
};

struct AudioStreamConfig {
  struct Source {
  } source;
  struct Encoder {
    struct VariableBitrateConfig {
      DataRate min_rate;
      DataRate max_rate;
    };
    absl::optional<VariableBitrateConfig> variable_rate;
    absl::optional<DataRate> fixed_rate;
  } encoder;
  struct Stream {
    bool bitrate_tracking = false;
    absl::optional<uint32_t> ssrc;
  } stream;
  struct Render {
    std::string sync_group;
  } render;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_SCENARIO_CONFIG_H_
