/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>

#include "absl/memory/memory.h"
#include "modules/congestion_controller/pcc/pcc_network_controller.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace pcc {
namespace {
constexpr int64_t kInitialRttMs = 200;
constexpr int64_t kInitialBandwidthKbps = 300;
constexpr double kMonitorIntervalDurationRatio = 1;
constexpr double kDefaultSamplingStep = 0.05;
constexpr double kTimeoutRatio = 4;
constexpr double kAlphaForRtt = 0.9;
constexpr double kSlowStartModeIncrease = 2;

constexpr double kAlphaForPacketInterval = 0.9;
constexpr int64_t kMinPacketsNumberPerInterval = 10;
const TimeDelta kMinDurationOfMonitorInterval = TimeDelta::ms(0);
const TimeDelta kStartupDuration = TimeDelta::ms(0);
constexpr double kMinRateChangeBps = 0;

// Bitrate controller constants.
constexpr double kInitialConversionFactor = 1;
constexpr double kInitialDynamicBoundary = 0.05;
constexpr double kDynamicBoundaryIncrement = 0.1;
// Utility function parameters.
constexpr double kRttGradientCoefficientBps = 900;
constexpr double kLossCoefficientBps = 11.35;
constexpr double kThroughputCoefficient = 1;
constexpr double kThroughputPower = 0.9;
constexpr double kRttGradientThreshold = 0.01;
constexpr double kDelayGradientNegativeBound = 100000;
constexpr double kLossRateThreshold = 0;

constexpr int64_t kNumberOfPacketsToKeep = 0;
const uint64_t kRandomSeed = 100;

const char kPccConfigTrial[] = "WebRTC-BwePccConfig";

std::unique_ptr<PccUtilityFunctionInterface> CreateUtilityFunction(
    bool is_modified,
    double rtt_gradient_coefficient,
    double loss_coefficient,
    double throughput_coefficient,
    double throughput_power,
    double rtt_gradient_threshold,
    double rtt_gradient_negative_bound,
    double loss_rate_threshold) {
  if (is_modified) {
    return absl::make_unique<ModifiedVivaceUtilityFunction>(
        rtt_gradient_coefficient, loss_coefficient, throughput_coefficient,
        throughput_power, rtt_gradient_threshold, rtt_gradient_negative_bound,
        loss_rate_threshold);
  }
  return absl::make_unique<VivaceUtilityFunction>(
      rtt_gradient_coefficient, loss_coefficient, throughput_coefficient,
      throughput_power, rtt_gradient_threshold, rtt_gradient_negative_bound,
      loss_rate_threshold);
}
}  // namespace

PccNetworkController::PccControllerConfig::PccControllerConfig(
    std::string field_trial)
    : alpha_for_packet_interval("alpha_for_packet_interval",
                                kAlphaForPacketInterval),
      default_bandwidth("default_bandwidth",
                        DataRate::kbps(kInitialBandwidthKbps)),
      initial_rtt_ms("initial_rtt_ms", kInitialRttMs),
      monitor_interval_timeout_ratio("monitor_interval_timeout_ratio",
                                     kTimeoutRatio),
      monitor_interval_length_strategy(
          "monitor_interval_length_strategy",
          MonitorIntervalLengthStrategy::kAdaptive,
          {{"kAdaptive", MonitorIntervalLengthStrategy::kAdaptive},
           {"kFixed", MonitorIntervalLengthStrategy::kFixed}}),
      monitor_interval_duration_ratio("monitor_interval_duration_ratio",
                                      kMonitorIntervalDurationRatio),
      min_duration_of_monitor_interval("min_duration_of_monitor_interval",
                                       kMinDurationOfMonitorInterval),
      sampling_step("sampling_step", kDefaultSamplingStep),
      min_rate_change_bps("min_rate_change_bps", kMinRateChangeBps),
      min_packets_number_per_interval("min_packets_number_per_interval",
                                      kMinPacketsNumberPerInterval),
      startup_duration("startup_duration", kStartupDuration),
      slow_start_increase_factor("slow_start_increase_factor",
                                 kSlowStartModeIncrease),
      initial_conversion_factor("initial_conversion_factor",
                                kInitialConversionFactor),
      initial_dynamic_boundary("initial_dynamic_boundary",
                               kInitialDynamicBoundary),
      dynamic_boundary_increment("dynamic_boundary_increment",
                                 kDynamicBoundaryIncrement),
      is_modified_utility_function("is_modified_utility_function", false),
      rtt_gradient_coefficient("rtt_gradient_coefficient",
                               kRttGradientCoefficientBps),
      loss_coefficient("loss_coefficient", kLossCoefficientBps),
      throughput_coefficient("throughput_coefficient", kThroughputCoefficient),
      throughput_power("throughput_power", kThroughputPower),
      rtt_gradient_threshold("rtt_gradient_threshold", kRttGradientThreshold),
      rtt_gradient_negative_bound("rtt_gradient_negative_bound",
                                  kDelayGradientNegativeBound),
      loss_rate_threshold("loss_rate_threshold", kLossRateThreshold),
      number_of_packets_to_keep("number_of_packets_to_keep",
                                kNumberOfPacketsToKeep) {
  ParseFieldTrial({&alpha_for_packet_interval,
                   &default_bandwidth,
                   &initial_rtt_ms,
                   &monitor_interval_timeout_ratio,
                   &monitor_interval_length_strategy,
                   &monitor_interval_duration_ratio,
                   &min_duration_of_monitor_interval,
                   &sampling_step,
                   &min_rate_change_bps,
                   &min_packets_number_per_interval,
                   &startup_duration,
                   &slow_start_increase_factor,
                   &loss_rate_threshold,
                   &initial_conversion_factor,
                   &initial_dynamic_boundary,
                   &dynamic_boundary_increment,
                   &rtt_gradient_coefficient,
                   &loss_coefficient,
                   &throughput_coefficient,
                   &throughput_power,
                   &rtt_gradient_threshold,
                   &rtt_gradient_negative_bound,
                   &number_of_packets_to_keep},
                  field_trial);
}
PccNetworkController::PccControllerConfig::~PccControllerConfig() = default;
PccNetworkController::PccControllerConfig::PccControllerConfig(
    const PccControllerConfig&) = default;
PccNetworkController::PccControllerConfig
PccNetworkController::PccControllerConfig::FromTrial() {
  return PccControllerConfig(
      webrtc::field_trial::FindFullName(kPccConfigTrial));
}

PccNetworkController::PccNetworkController(NetworkControllerConfig config)
    : config_(PccControllerConfig::FromTrial()),
      start_time_(Timestamp::Infinity()),
      last_sent_packet_time_(Timestamp::Infinity()),
      smoothed_packets_sending_interval_(TimeDelta::Zero()),
      mode_(Mode::kStartup),
      bandwidth_estimate_(config_.default_bandwidth),
      rtt_tracker_(TimeDelta::ms(static_cast<int64_t>(config_.initial_rtt_ms)),
                   kAlphaForRtt),
      monitor_interval_timeout_(rtt_tracker_.GetRtt() *
                                config_.monitor_interval_timeout_ratio),
      min_rate_have_multiplicative_rate_change_(
          DataRate::bps(config_.min_rate_change_bps / config_.sampling_step)),
      bitrate_controller_(
          config_.initial_conversion_factor,
          config_.initial_dynamic_boundary,
          config_.dynamic_boundary_increment,
          CreateUtilityFunction(config_.is_modified_utility_function,
                                config_.rtt_gradient_coefficient,
                                config_.loss_coefficient,
                                config_.throughput_coefficient,
                                config_.throughput_power,
                                config_.rtt_gradient_threshold,
                                config_.rtt_gradient_negative_bound,
                                config_.loss_rate_threshold)),
      monitor_intervals_duration_(TimeDelta::Zero()),
      complete_feedback_monitor_interval_number_(0),
      random_generator_(kRandomSeed) {
  if (config.starting_bandwidth.IsFinite()) {
    bandwidth_estimate_ = config.starting_bandwidth;
  }
}

PccNetworkController::~PccNetworkController() {}

NetworkControlUpdate PccNetworkController::CreateRateUpdate(
    Timestamp at_time) const {
  DataRate sending_rate = DataRate::Zero();
  if (monitor_intervals_.empty() ||
      (monitor_intervals_.size() >= monitor_intervals_bitrates_.size() &&
       at_time >= monitor_intervals_.back().GetEndTime())) {
    sending_rate = bandwidth_estimate_;
  } else {
    sending_rate = monitor_intervals_.back().GetTargetSendingRate();
  }
  // Set up config when sending rate is computed.
  NetworkControlUpdate update;

  // Set up target rate to encoder.
  TargetTransferRate target_rate_msg;
  target_rate_msg.network_estimate.at_time = at_time;
  target_rate_msg.network_estimate.round_trip_time = rtt_tracker_.GetRtt();
  target_rate_msg.network_estimate.bandwidth = bandwidth_estimate_;
  // TODO(koloskova): Add correct estimate.
  target_rate_msg.network_estimate.loss_rate_ratio = 0;
  target_rate_msg.network_estimate.bwe_period =
      config_.monitor_interval_duration_ratio * rtt_tracker_.GetRtt();

  target_rate_msg.target_rate = sending_rate;
  update.target_rate = target_rate_msg;

  // Set up pacing/padding target rate.
  PacerConfig pacer_config;
  pacer_config.at_time = at_time;
  pacer_config.time_window = TimeDelta::ms(1);
  pacer_config.data_window = sending_rate * pacer_config.time_window;
  pacer_config.pad_window = sending_rate * pacer_config.time_window;

  update.pacer_config = pacer_config;
  return update;
}

NetworkControlUpdate PccNetworkController::OnSentPacket(SentPacket msg) {
  // Start new monitor interval if previous has finished.
  // Monitor interval is initialized in OnProcessInterval function.
  if (start_time_.IsInfinite()) {
    start_time_ = msg.send_time;
    monitor_intervals_duration_ = config_.startup_duration;
    RTC_DCHECK(mode_ == Mode::kStartup);
    monitor_intervals_bitrates_ = {bandwidth_estimate_};
    monitor_intervals_.emplace_back(bandwidth_estimate_, msg.send_time,
                                    monitor_intervals_duration_);
    complete_feedback_monitor_interval_number_ = 0;
  }
  if (last_sent_packet_time_.IsFinite()) {
    smoothed_packets_sending_interval_ =
        (msg.send_time - last_sent_packet_time_) *
            config_.alpha_for_packet_interval +
        (1 - config_.alpha_for_packet_interval) *
            smoothed_packets_sending_interval_;
  }
  last_sent_packet_time_ = msg.send_time;
  if (!monitor_intervals_.empty() &&
      msg.send_time >= monitor_intervals_.back().GetEndTime() &&
      monitor_intervals_bitrates_.size() > monitor_intervals_.size()) {
    // Start new monitor interval.
    monitor_intervals_.emplace_back(
        monitor_intervals_bitrates_[monitor_intervals_.size()], msg.send_time,
        monitor_intervals_duration_);
  }
  if (IsTimeoutExpired(msg.send_time)) {
    DataSize received_size = DataSize::Zero();
    for (size_t i = 1; i < last_received_packets_.size(); ++i) {
      received_size += last_received_packets_[i].sent_packet->size;
    }
    TimeDelta sending_time = TimeDelta::Zero();
    if (last_received_packets_.size() > 0)
      sending_time = last_received_packets_.back().receive_time -
                     last_received_packets_.front().receive_time;
    DataRate receiving_rate = bandwidth_estimate_;
    if (sending_time > TimeDelta::Zero())
      receiving_rate = received_size / sending_time;
    bandwidth_estimate_ =
        std::min<DataRate>(bandwidth_estimate_ * 0.5, receiving_rate);
    if (mode_ == Mode::kSlowStart)
      mode_ = Mode::kOnlineLearning;
  }
  if (mode_ == Mode::kStartup &&
      msg.send_time - start_time_ >= config_.startup_duration) {
    DataSize received_size = DataSize::Zero();
    for (size_t i = 1; i < last_received_packets_.size(); ++i) {
      received_size += last_received_packets_[i].sent_packet->size;
    }
    TimeDelta sending_time = TimeDelta::Zero();
    if (last_received_packets_.size() > 0)
      sending_time = last_received_packets_.back().receive_time -
                     last_received_packets_.front().receive_time;
    DataRate receiving_rate = bandwidth_estimate_;
    if (sending_time > TimeDelta::Zero())
      receiving_rate = received_size / sending_time;
    bandwidth_estimate_ = receiving_rate;
    monitor_intervals_.clear();
    mode_ = Mode::kSlowStart;
    monitor_intervals_duration_ = ComputeMonitorIntervalsDuration();
    monitor_intervals_bitrates_ = {bandwidth_estimate_};
    monitor_intervals_.emplace_back(bandwidth_estimate_, msg.send_time,
                                    monitor_intervals_duration_);
    bandwidth_estimate_ =
        bandwidth_estimate_ * (1 / config_.slow_start_increase_factor);
    complete_feedback_monitor_interval_number_ = 0;
    return CreateRateUpdate(msg.send_time);
  }
  if (IsFeedbackCollectionDone() || IsTimeoutExpired(msg.send_time)) {
    // Creating new monitor intervals.
    monitor_intervals_.clear();
    monitor_interval_timeout_ =
        rtt_tracker_.GetRtt() * config_.monitor_interval_timeout_ratio;
    monitor_intervals_duration_ = ComputeMonitorIntervalsDuration();
    complete_feedback_monitor_interval_number_ = 0;
    // Compute bitrates and start first monitor interval.
    if (mode_ == Mode::kSlowStart) {
      monitor_intervals_bitrates_ = {config_.slow_start_increase_factor *
                                     bandwidth_estimate_};
      monitor_intervals_.emplace_back(
          config_.slow_start_increase_factor * bandwidth_estimate_,
          msg.send_time, monitor_intervals_duration_);
    } else {
      RTC_DCHECK(mode_ == Mode::kOnlineLearning || mode_ == Mode::kDoubleCheck);
      monitor_intervals_.clear();
      int64_t sign = 2 * (random_generator_.Rand(0, 1) % 2) - 1;
      RTC_DCHECK_GE(sign, -1);
      RTC_DCHECK_LE(sign, 1);
      if (bandwidth_estimate_ >= min_rate_have_multiplicative_rate_change_) {
        monitor_intervals_bitrates_ = {
            bandwidth_estimate_ * (1 + sign * config_.sampling_step),
            bandwidth_estimate_ * (1 - sign * config_.sampling_step)};
      } else {
        monitor_intervals_bitrates_ = {
            DataRate::bps(std::max<double>(
                bandwidth_estimate_.bps() + sign * config_.min_rate_change_bps,
                0)),
            DataRate::bps(std::max<double>(
                bandwidth_estimate_.bps() - sign * config_.min_rate_change_bps,
                0))};
      }
      monitor_intervals_.emplace_back(monitor_intervals_bitrates_[0],
                                      msg.send_time,
                                      monitor_intervals_duration_);
    }
  }
  return CreateRateUpdate(msg.send_time);
}

TimeDelta PccNetworkController::ComputeMonitorIntervalsDuration() const {
  TimeDelta monitor_intervals_duration = TimeDelta::Zero();
  if (config_.monitor_interval_length_strategy ==
      MonitorIntervalLengthStrategy::kAdaptive) {
    monitor_intervals_duration = std::max(
        rtt_tracker_.GetRtt() * config_.monitor_interval_duration_ratio,
        smoothed_packets_sending_interval_ *
            config_.min_packets_number_per_interval);
  } else {
    RTC_DCHECK(config_.monitor_interval_length_strategy ==
               MonitorIntervalLengthStrategy::kFixed);
    monitor_intervals_duration = smoothed_packets_sending_interval_ *
                                 config_.min_packets_number_per_interval;
  }
  monitor_intervals_duration = std::max<TimeDelta>(
      config_.min_duration_of_monitor_interval, monitor_intervals_duration);
  return monitor_intervals_duration;
}

bool PccNetworkController::IsTimeoutExpired(Timestamp current_time) const {
  if (complete_feedback_monitor_interval_number_ >= monitor_intervals_.size()) {
    return false;
  }
  return current_time -
             monitor_intervals_[complete_feedback_monitor_interval_number_]
                 .GetEndTime() >=
         monitor_interval_timeout_;
}

bool PccNetworkController::IsFeedbackCollectionDone() const {
  return complete_feedback_monitor_interval_number_ >=
         monitor_intervals_bitrates_.size();
}

NetworkControlUpdate PccNetworkController::OnTransportPacketsFeedback(
    TransportPacketsFeedback msg) {
  // Save packets to last_received_packets_ array.
  for (const PacketResult& packet_result : msg.ReceivedWithSendInfo()) {
    last_received_packets_.push_back(packet_result);
  }
  while (last_received_packets_.size() > config_.number_of_packets_to_keep) {
    last_received_packets_.pop_front();
  }
  rtt_tracker_.OnPacketsFeedback(msg.PacketsWithFeedback(), msg.feedback_time);
  // Skip rate update in case when online learning mode just started, but
  // corresponding monitor intervals were not started yet.
  if (mode_ == Mode::kOnlineLearning &&
      monitor_intervals_bitrates_.size() < 2) {
    return NetworkControlUpdate();
  }
  if (!IsFeedbackCollectionDone() && !monitor_intervals_.empty()) {
    while (complete_feedback_monitor_interval_number_ <
           monitor_intervals_.size()) {
      monitor_intervals_[complete_feedback_monitor_interval_number_]
          .OnPacketsFeedback(msg.PacketsWithFeedback());
      if (!monitor_intervals_[complete_feedback_monitor_interval_number_]
               .IsFeedbackCollectionDone())
        break;
      ++complete_feedback_monitor_interval_number_;
    }
  }
  if (IsFeedbackCollectionDone()) {
    if (mode_ == Mode::kDoubleCheck) {
      mode_ = Mode::kOnlineLearning;
    } else if (NeedDoubleCheckMeasurments()) {
      mode_ = Mode::kDoubleCheck;
    }
    if (mode_ != Mode::kDoubleCheck)
      UpdateSendingRateAndMode();
  }
  return NetworkControlUpdate();
}

bool PccNetworkController::NeedDoubleCheckMeasurments() const {
  if (mode_ == Mode::kSlowStart) {
    return false;
  }
  double first_loss_rate =
      monitor_intervals_[0].GetLossRate(config_.loss_rate_threshold);
  double second_loss_rate =
      monitor_intervals_[1].GetLossRate(config_.loss_rate_threshold);
  DataRate first_bitrate = monitor_intervals_[0].GetTargetSendingRate();
  DataRate second_bitrate = monitor_intervals_[1].GetTargetSendingRate();
  if ((first_bitrate.bps() - second_bitrate.bps()) *
          (first_loss_rate - second_loss_rate) <
      0) {
    return true;
  }
  return false;
}

void PccNetworkController::UpdateSendingRateAndMode() {
  if (monitor_intervals_.empty() || !IsFeedbackCollectionDone()) {
    return;
  }
  if (mode_ == Mode::kSlowStart) {
    DataRate old_bandwidth_estimate = bandwidth_estimate_;
    bandwidth_estimate_ =
        bitrate_controller_
            .ComputeRateUpdateForSlowStartMode(monitor_intervals_[0])
            .value_or(bandwidth_estimate_);
    if (bandwidth_estimate_ <= old_bandwidth_estimate)
      mode_ = Mode::kOnlineLearning;
  } else {
    RTC_DCHECK(mode_ == Mode::kOnlineLearning);
    bandwidth_estimate_ =
        bitrate_controller_.ComputeRateUpdateForOnlineLearningMode(
            monitor_intervals_, bandwidth_estimate_);
  }
}

NetworkControlUpdate PccNetworkController::OnNetworkAvailability(
    NetworkAvailability msg) {
  return NetworkControlUpdate();
}

NetworkControlUpdate PccNetworkController::OnNetworkRouteChange(
    NetworkRouteChange msg) {
  return NetworkControlUpdate();
}

NetworkControlUpdate PccNetworkController::OnProcessInterval(
    ProcessInterval msg) {
  return CreateRateUpdate(msg.at_time);
}

NetworkControlUpdate PccNetworkController::OnTargetRateConstraints(
    TargetRateConstraints msg) {
  return NetworkControlUpdate();
}

NetworkControlUpdate PccNetworkController::OnRemoteBitrateReport(
    RemoteBitrateReport) {
  return NetworkControlUpdate();
}

NetworkControlUpdate PccNetworkController::OnRoundTripTimeUpdate(
    RoundTripTimeUpdate) {
  return NetworkControlUpdate();
}

NetworkControlUpdate PccNetworkController::OnTransportLossReport(
    TransportLossReport) {
  return NetworkControlUpdate();
}

NetworkControlUpdate PccNetworkController::OnStreamsConfig(StreamsConfig) {
  return NetworkControlUpdate();
}

}  // namespace pcc
}  // namespace webrtc
