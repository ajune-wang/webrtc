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

#include "p2p/base/p2ptransportchannel.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

BasicRegatheringController::BasicRegatheringController(
    cricket::P2PTransportChannel* ice_transport)
    : ice_transport_(ice_transport), rand_(rtc::SystemTimeNanos()) {
  RTC_DCHECK(ice_transport_);
  thread_ = ice_transport->thread();
}

BasicRegatheringController::~BasicRegatheringController() = default;

void BasicRegatheringController::ScheduleRegatheringOnAllNetworks(
    int delay_ms,
    bool repeated) {
  rtc::Optional<int> next_schedule_delay_ms(rtc::nullopt);
  if (repeated) {
    // The interval to regather on all networks is randomized.
    next_schedule_delay_ms = regather_on_all_networks_interval();
  }
  invoker_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(&BasicRegatheringController::RegatherOnAllNetworks, this,
                repeated, next_schedule_delay_ms),
      delay_ms);
}

void BasicRegatheringController::RegatherOnAllNetworks(
    bool repeated,
    rtc::Optional<int> next_schedule_delay_ms) {
  if (ice_transport_->HasClearedAllocatorSession()) {
    last_regathering_ms_ = rtc::TimeMillis();
    ice_transport_->allocator_session()->RegatherOnAllNetworks();
  }
  if (repeated) {
    RTC_DCHECK(next_schedule_delay_ms);
    ScheduleRegatheringOnAllNetworks(next_schedule_delay_ms.value(), true);
  }
}

void BasicRegatheringController::ScheduleRegatheringOnFailedNetworks(
    int delay_ms,
    bool repeated) {
  rtc::Optional<int> next_schedule_delay_ms(rtc::nullopt);
  if (repeated) {
    // The interval to regather on all networks is fixed.
    next_schedule_delay_ms = delay_ms;
  }
  invoker_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(&BasicRegatheringController::RegatherOnFailedNetworks, this,
                repeated, next_schedule_delay_ms),
      regather_on_failed_networks_interval());
}

void BasicRegatheringController::RegatherOnFailedNetworks(
    bool repeated,
    rtc::Optional<int> next_schedule_delay_ms) {
  // Only re-gather when the current session is in the CLEARED state (i.e., not
  // running or stopped). It is only possible to enter this state when we gather
  // continually, so there is an implicit check on continual gathering here.
  if (ice_transport_->HasClearedAllocatorSession()) {
    last_regathering_ms_ = rtc::TimeMillis();
    ice_transport_->allocator_session()->RegatherOnFailedNetworks();
  }
  if (repeated) {
    RTC_DCHECK(next_schedule_delay_ms);
    ScheduleRegatheringOnFailedNetworks(next_schedule_delay_ms.value(), true);
  }
}

void BasicRegatheringController::CancelScheduledRegathering() {
  invoker_.Clear();
}

bool BasicRegatheringController::ShouldRegatherOnAllNetworks() {
  if (rtc::TimeMillis() <
      last_regathering_ms_ + min_regathering_interval_ms_or_default()) {
    return false;
  }
  if (ice_transport_->TooManyWeakSelectedCandidatePairs() ||
      ice_transport_->TooLargePingRttOverSelectedCandidatePair()) {
    return true;
  }
  return false;
}

int BasicRegatheringController::regather_on_all_networks_interval() {
  return SampleRegatherAllNetworksInterval();
}

int BasicRegatheringController::regather_on_failed_networks_interval() {
  return ice_transport_->config()
      .regather_on_failed_networks_interval_or_default();
}

int BasicRegatheringController::SampleRegatherAllNetworksInterval() {
  auto interval = ice_transport_->config().regather_all_networks_interval_range;
  RTC_DCHECK(interval);
  return rand_.Rand(interval->min(), interval->max());
}

}  // namespace webrtc
