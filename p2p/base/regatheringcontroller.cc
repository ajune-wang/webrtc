/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/regatheringcontroller.h"

namespace webrtc {

using Config = BasicRegatheringController::Config;

Config::Config(const rtc::Optional<rtc::IntervalRange>&
                   regather_on_all_networks_interval_range,
               int regather_on_failed_networks_interval)
    : regather_on_all_networks_interval_range(
          regather_on_all_networks_interval_range),
      regather_on_failed_networks_interval(
          regather_on_failed_networks_interval) {}

Config::Config(const Config& other) = default;

Config::~Config() = default;
Config& Config::operator=(const Config& other) = default;

BasicRegatheringController::BasicRegatheringController(
    const Config& config,
    cricket::IceTransportInternal* ice_transport,
    rtc::Thread* thread)
    : config_(config),
      ice_transport_(ice_transport),
      thread_(thread),
      rand_(rtc::SystemTimeNanos()) {
  RTC_DCHECK(ice_transport_);
  RTC_DCHECK(thread_);
  ice_transport_->SignalStateChanged.connect(
      this, &BasicRegatheringController::OnIceTransportStateChanged);
  ice_transport->SignalWritableState.connect(
      this, &BasicRegatheringController::OnIceTransportWritableState);
  ice_transport->SignalReceivingState.connect(
      this, &BasicRegatheringController::OnIceTransportReceivingState);
  ice_transport->SignalNetworkRouteChanged.connect(
      this, &BasicRegatheringController::OnIceTransportNetworkRouteChanged);
}

BasicRegatheringController::~BasicRegatheringController() = default;

void BasicRegatheringController::Start() {
  ScheduleRegatheringOnFailedNetworks(true);
  if (config_.regather_on_all_networks_interval_range) {
    ScheduleRegatheringOnAllNetworks(true);
  }
}

void BasicRegatheringController::SetConfig(const Config& config) {
  bool need_cancel_and_maybe_reschedule_on_all_networks =
      has_recurring_schedule_on_all_networks_ &&
      (config_.regather_on_all_networks_interval_range !=
       config.regather_on_all_networks_interval_range);
  bool need_cancel_and_reschedule_on_failed_networks =
      has_recurring_schedule_on_failed_networks_ &&
      (config_.regather_on_failed_networks_interval !=
       config.regather_on_failed_networks_interval);
  config_ = config;
  if (need_cancel_and_maybe_reschedule_on_all_networks) {
    CancelScheduledRegatheringOnAllNetworks();
    if (config_.regather_on_all_networks_interval_range) {
      ScheduleRegatheringOnAllNetworks(true);
    }
  }
  if (need_cancel_and_reschedule_on_failed_networks) {
    CancelScheduledRegatheringOnFailedNetworks();
    ScheduleRegatheringOnFailedNetworks(true);
  }
}

void BasicRegatheringController::ScheduleRegatheringOnAllNetworks(
    bool repeated) {
  RTC_DCHECK(config_.regather_on_all_networks_interval_range &&
             config_.regather_on_all_networks_interval_range.value().min() >=
                 0);
  int delay_ms = SampleRegatherAllNetworksInterval(
      config_.regather_on_all_networks_interval_range.value());
  if (repeated) {
    CancelScheduledRegatheringOnAllNetworks();
    has_recurring_schedule_on_all_networks_ = true;
  }
  invoker_for_all_networks_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(
          &BasicRegatheringController::RegatherOnAllNetworksIfDoneGathering,
          this, repeated),
      delay_ms);
}

void BasicRegatheringController::RegatherOnAllNetworksIfDoneGathering(
    bool repeated) {
  // Only regather when the current session is in the CLEARED state (i.e., not
  // running or stopped). It is only possible to enter this state when we gather
  // continually, so there is an implicit check on continual gathering here.
  if (allocator_session_ && allocator_session_->IsCleared()) {
    last_regathering_ms_on_all_networks_ = rtc::TimeMillis();
    allocator_session_->RegatherOnAllNetworks();
  }
  if (repeated) {
    ScheduleRegatheringOnAllNetworks(true);
  }
}

void BasicRegatheringController::ScheduleRegatheringOnFailedNetworks(
    bool repeated) {
  RTC_DCHECK(config_.regather_on_failed_networks_interval >= 0);
  if (repeated) {
    CancelScheduledRegatheringOnFailedNetworks();
    has_recurring_schedule_on_failed_networks_ = true;
  }
  invoker_for_failed_networks_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(
          &BasicRegatheringController::RegatherOnFailedNetworksIfDoneGathering,
          this, repeated),
      config_.regather_on_failed_networks_interval);
}

void BasicRegatheringController::ScheduleOneTimeRegatheringOnAllNetworks(
    const rtc::IntervalRange& range) {
  int delay_ms = SampleRegatherAllNetworksInterval(range);
  invoker_for_one_time_regathering_on_all_networks_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(
          &BasicRegatheringController::RegatherOnAllNetworksIfDoneGathering,
          this, false),
      delay_ms);
}

void BasicRegatheringController::RegatherOnFailedNetworksIfDoneGathering(
    bool repeated) {
  // Only regather when the current session is in the CLEARED state (i.e., not
  // running or stopped). It is only possible to enter this state when we gather
  // continually, so there is an implicit check on continual gathering here.
  if (allocator_session_ && allocator_session_->IsCleared()) {
    allocator_session_->RegatherOnFailedNetworks();
  }
  if (repeated) {
    ScheduleRegatheringOnFailedNetworks(true);
  }
}

void BasicRegatheringController::CancelScheduledRegatheringOnAllNetworks() {
  invoker_for_all_networks_.Clear();
  has_recurring_schedule_on_all_networks_ = false;
}

void BasicRegatheringController::CancelScheduledRegatheringOnFailedNetworks() {
  invoker_for_failed_networks_.Clear();
  has_recurring_schedule_on_failed_networks_ = false;
}

void BasicRegatheringController::OnIceTransportStateChanged(
    cricket::IceTransportInternal*) {
  MaybeRegatherOnAllNetworks();
}

void BasicRegatheringController::OnIceTransportWritableState(
    rtc::PacketTransportInternal* transport) {
  if (!transport->writable()) {
    if (last_time_ms_writable_) {
      // If we are changing from writable to not writable, consider regathering.
      MaybeRegatherOnAllNetworks();
    }
  }
  last_time_ms_writable_ = rtc::TimeMillis();
}

void BasicRegatheringController::OnIceTransportReceivingState(
    rtc::PacketTransportInternal* transport) {
  if (!transport->receiving()) {
    MaybeRegatherOnAllNetworks();
  }
}

void BasicRegatheringController::OnIceTransportNetworkRouteChanged(
    rtc::Optional<rtc::NetworkRoute>) {
  MaybeRegatherOnAllNetworks();
}

void BasicRegatheringController::MaybeRegatherOnAllNetworks() {
  if (!ice_transport_->GetIceConfig().gather_autonomously()) {
    return;
  }
  IceTransportStats stats;
  ice_transport_->GetStats(&stats);
  if (ShouldRegatherOnAllNetworks(stats)) {
    RTC_LOG(INFO) << "Start autonomous regathering of local candidates.";
    // Schedule regathering immediately.
    ScheduleOneTimeRegatheringOnAllNetworks(rtc::IntervalRange() /* no delay*/);
    return;
  }
  invoker_for_one_time_regathering_on_all_networks_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(&BasicRegatheringController::MaybeRegatherOnAllNetworks, this),
      min_regathering_interval_ms_or_default());
}

bool BasicRegatheringController::TooManyWeakSelectedCandidatePairs(
    const IceTransportStats& stats) const {
  return stats.num_continual_switchings_to_weak_candidate_pairs >
         cricket::kMinNumSwitchingsToWeakCandidatePairsBeforeRegathering;
}

bool BasicRegatheringController::TooLargePingRttOverSelectedCandidatePair(
    const IceTransportStats& stats) const {
  return stats.selected_candidate_pair_connectivity_check_rtt_ms >
         cricket::kMinRttMsOverSelectedCandidatePairBeforeRegathering;
}

bool BasicRegatheringController::HadSelectedCandidatePair(
    const IceTransportStats& stats) const {
  return stats.had_selected_candidate_pair;
}

bool BasicRegatheringController::HasActiveCandidatePair(
    const IceTransportStats& stats) const {
  return stats.num_active_candidate_pairs != 0;
}

bool BasicRegatheringController::HasWritableCandidatePair(
    const IceTransportStats& stats) const {
  return stats.num_writable_candidate_pairs != 0;
}

bool BasicRegatheringController::ShouldRegatherOnAllNetworks(
    const IceTransportStats& stats) {
  if (last_regathering_ms_on_all_networks_ &&
      rtc::TimeMillis() < last_regathering_ms_on_all_networks_.value() +
                              min_regathering_interval_ms_or_default()) {
    return false;
  }
  if (HadSelectedCandidatePair(stats) &&
      (TooManyWeakSelectedCandidatePairs(stats) ||
       TooLargePingRttOverSelectedCandidatePair(stats) ||
       !HasActiveCandidatePair(stats) || !HasWritableCandidatePair(stats))) {
    return true;
  }
  return false;
}

int BasicRegatheringController::SampleRegatherAllNetworksInterval(
    const rtc::IntervalRange& range) {
  return rand_.Rand(range.min(), range.max());
}

}  // namespace webrtc
