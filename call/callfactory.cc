/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/callfactory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "call/call.h"
#include "call/degraded_call.h"
#include "call/fake_network_pipe.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
bool ParseString(const std::string& string, int* out) {
  return !string.empty() && sscanf(string.c_str(), "%d", out) == 1;
}
bool ParseString(const std::string& string, int64_t* out) {
  return !string.empty() && sscanf(string.c_str(), "%ld", out) == 1;
}
bool ParseString(const std::string& string, size_t* out) {
  return !string.empty() && sscanf(string.c_str(), "%lu", out) == 1;
}
bool ParseString(const std::string& string, bool* out) {
  if (string.empty()) {
    return false;
  }
  if (string.find("true") == 0) {
    return true;
  }
  int intval = 0;
  if (sscanf(string.c_str(), "%d", &intval) != 1) {
    return false;
  }
  *out = intval > 0;
  return true;
}

template <typename T, T webrtc::DefaultNetworkSimulationConfig::*param>
void ParseConfigParams(
    std::string exp_name,
    std::vector<webrtc::DefaultNetworkSimulationConfig>* configs) {
  std::string group = field_trial::FindFullName(exp_name);
  if (group == "")
    return;

  size_t current_pos = 0;
  size_t value_end_pos;
  size_t config_index = 0;
  do {
    value_end_pos = group.find(',', current_pos);
    if (value_end_pos == std::string::npos) {
      value_end_pos = group.length();
    }
    if (value_end_pos - current_pos > 0) {
      const std::string value_string =
          group.substr(current_pos, value_end_pos - current_pos);
      T value;
      if (ParseString(value_string, &value)) {
        if (configs->size() <= config_index) {
          configs->resize(config_index + 1);
        }
        configs->at(config_index).*param = value;
      } else {
        RTC_LOG(LS_WARNING) << "Unparsable value: " << value_string;
      }
    }
    current_pos = ++value_end_pos;
    ++config_index;
  } while (value_end_pos < group.length());
}

std::vector<webrtc::DefaultNetworkSimulationConfig> ParseDegradationConfig(
    bool send) {
  std::string exp_prefix = "WebRTCFakeNetwork";
  if (send) {
    exp_prefix += "Send";
  } else {
    exp_prefix += "Receive";
  }

  using Config = webrtc::DefaultNetworkSimulationConfig;
  std::vector<Config> configs;
  ParseConfigParams<size_t, &Config::queue_length_packets>(
      exp_prefix + "QueueLength", &configs);
  ParseConfigParams<int, &Config::queue_delay_ms>(exp_prefix + "DelayMs",
                                                  &configs);
  ParseConfigParams<int, &Config::delay_standard_deviation_ms>(
      exp_prefix + "DelayStdDevMs", &configs);
  ParseConfigParams<int, &Config::link_capacity_kbps>(
      exp_prefix + "CapacityKbps", &configs);
  ParseConfigParams<int, &Config::loss_percent>(exp_prefix + "LossPercent",
                                                &configs);
  ParseConfigParams<bool, &Config::allow_reordering>(
      exp_prefix + "AllowReordering", &configs);
  ParseConfigParams<int, &Config::avg_burst_loss_length>(
      exp_prefix + "AvgBurstLossLength", &configs);
  ParseConfigParams<int64_t, &Config::config_durations_ms>(
      exp_prefix + "ConfigDuration", &configs);

  return configs;
}
}  // namespace

Call* CallFactory::CreateCall(const Call::Config& config) {
  std::vector<webrtc::DefaultNetworkSimulationConfig> send_degradation_config =
      ParseDegradationConfig(true);
  std::vector<webrtc::DefaultNetworkSimulationConfig>
      receive_degradation_config = ParseDegradationConfig(false);

  if (!send_degradation_config.empty() || !receive_degradation_config.empty()) {
    return new DegradedCall(std::unique_ptr<Call>(Call::Create(config)),
                            send_degradation_config,
                            receive_degradation_config);
  }

  return Call::Create(config);
}

std::unique_ptr<CallFactoryInterface> CreateCallFactory() {
  return std::unique_ptr<CallFactoryInterface>(new CallFactory());
}

}  // namespace webrtc
