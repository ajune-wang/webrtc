/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/balanced_degradation_settings.h"
#include <limits>

#include "rtc_base/experiments/field_trial_list.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
constexpr char kFieldTrial[] = "WebRTC-Video-BalancedDegradationSettings";
constexpr int kMinFps = 1;
constexpr int kMaxFps = 100;

std::vector<BalancedDegradationSettings::Config> DefaultConfigs() {
  return {{320 * 240, 7, 0, 0, 0},
          {480 * 270, 10, 0, 0, 0},
          {640 * 480, 15, 0, 0, 0}};
}

bool IsValid(const std::vector<BalancedDegradationSettings::Config>& configs) {
  if (configs.size() <= 1) {
    RTC_LOG(LS_WARNING) << "Unsupported size, value ignored.";
    return false;
  }
  for (const auto& config : configs) {
    if (config.fps < kMinFps || config.fps > kMaxFps) {
      RTC_LOG(LS_WARNING) << "Unsupported fps setting, value ignored.";
      return false;
    }
  }
  for (size_t i = 1; i < configs.size(); ++i) {
    if (configs[i].pixels < configs[i - 1].pixels ||
        configs[i].fps < configs[i - 1].fps) {
      RTC_LOG(LS_WARNING) << "Invalid parameter value provided.";
      return false;
    }
  }
  return true;
}

std::vector<BalancedDegradationSettings::Config> GetValidOrDefault(
    const std::vector<BalancedDegradationSettings::Config>& configs) {
  if (IsValid(configs)) {
    return configs;
  }
  return DefaultConfigs();
}
}  // namespace

BalancedDegradationSettings::Config::Config() = default;

BalancedDegradationSettings::Config::Config(int pixels,
                                            int fps,
                                            int vp8_qp_high,
                                            int h264_qp_high,
                                            int generic_qp_high)
    : pixels(pixels),
      fps(fps),
      vp8_qp_high(vp8_qp_high),
      h264_qp_high(h264_qp_high),
      generic_qp_high(generic_qp_high) {}

BalancedDegradationSettings::BalancedDegradationSettings() {
  FieldTrialStructList<Config> configs(
      {FieldTrialStructMember("pixels", [](Config* c) { return &c->pixels; }),
       FieldTrialStructMember("fps", [](Config* c) { return &c->fps; }),
       FieldTrialStructMember("vp8_qp_high",
                              [](Config* c) { return &c->vp8_qp_high; }),
       FieldTrialStructMember("h264_qp_high",
                              [](Config* c) { return &c->h264_qp_high; }),
       FieldTrialStructMember("generic_qp_high",
                              [](Config* c) { return &c->generic_qp_high; })},
      {});

  ParseFieldTrial({&configs}, field_trial::FindFullName(kFieldTrial));

  configs_ = GetValidOrDefault(configs.Get());
  RTC_DCHECK_GT(configs_.size(), 1);
}

BalancedDegradationSettings::~BalancedDegradationSettings() {}

std::vector<BalancedDegradationSettings::Config>
BalancedDegradationSettings::GetConfigs() const {
  return configs_;
}

int BalancedDegradationSettings::MinFps(int pixels) const {
  for (const auto& config : configs_) {
    if (pixels <= config.pixels)
      return config.fps;
  }
  return std::numeric_limits<int>::max();
}

int BalancedDegradationSettings::MaxFps(int pixels) const {
  for (size_t i = 0; i < configs_.size() - 1; ++i) {
    if (pixels <= configs_[i].pixels)
      return configs_[i + 1].fps;
  }
  return std::numeric_limits<int>::max();
}

absl::optional<int> BalancedDegradationSettings::QpHighThreshold(
    VideoCodecType type,
    int pixels) const {
  switch (type) {
    case kVideoCodecVP8:
      return Vp8QpHigh(pixels);
    case kVideoCodecH264:
      return H264QpHigh(pixels);
    case kVideoCodecGeneric:
      return GenericQpHigh(pixels);
    default:
      return absl::nullopt;
  }
}

absl::optional<int> BalancedDegradationSettings::Vp8QpHigh(int pixels) const {
  for (const auto& config : configs_) {
    if (pixels <= config.pixels) {
      return (config.vp8_qp_high > 0) ? absl::optional<int>(config.vp8_qp_high)
                                      : absl::nullopt;
    }
  }
  return absl::nullopt;
}

absl::optional<int> BalancedDegradationSettings::H264QpHigh(int pixels) const {
  for (const auto& config : configs_) {
    if (pixels <= config.pixels) {
      return (config.h264_qp_high > 0)
                 ? absl::optional<int>(config.h264_qp_high)
                 : absl::nullopt;
    }
  }
  return absl::nullopt;
}

absl::optional<int> BalancedDegradationSettings::GenericQpHigh(
    int pixels) const {
  for (const auto& config : configs_) {
    if (pixels <= config.pixels) {
      return (config.generic_qp_high > 0)
                 ? absl::optional<int>(config.generic_qp_high)
                 : absl::nullopt;
    }
  }
  return absl::nullopt;
}

}  // namespace webrtc
