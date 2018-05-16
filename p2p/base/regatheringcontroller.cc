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

BasicRegatheringController::BasicRegatheringController(rtc::Thread* thread)
    : thread_(thread), rand_(rtc::SystemTimeNanos()) {}

BasicRegatheringController::~BasicRegatheringController() = default;

void BasicRegatheringController::ScheduleRegatheringOnAllNetworks(
    rtc::IntervalRange delay_ms_range,
    bool repeated) {
  int delay_ms = SampleRegatherAllNetworksInterval(delay_ms_range);
  rtc::Optional<rtc::IntervalRange> next_schedule_delay_ms_range(rtc::nullopt);
  if (repeated) {
    CancelScheduledRegathering();
    next_schedule_delay_ms_range = delay_ms_range;
  }
  invoker_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(
          &BasicRegatheringController::RegatherOnAllNetworksIfDoneGathering,
          this, repeated, next_schedule_delay_ms_range),
      delay_ms);
}

void BasicRegatheringController::RegatherOnAllNetworksIfDoneGathering(
    bool repeated,
    rtc::Optional<rtc::IntervalRange> next_schedule_delay_ms_range) {
  // Only re-gather when the current session is in the CLEARED state (i.e., not
  // running or stopped). It is only possible to enter this state when we gather
  // continually, so there is an implicit check on continual gathering here.
  if (allocator_session_ && allocator_session_->IsCleared()) {
    allocator_session_->RegatherOnAllNetworks();
  }
  if (repeated) {
    RTC_DCHECK(next_schedule_delay_ms_range);
    ScheduleRegatheringOnAllNetworks(next_schedule_delay_ms_range.value(),
                                     true);
  }
}

void BasicRegatheringController::ScheduleRegatheringOnFailedNetworks(
    int delay_ms,
    bool repeated) {
  rtc::Optional<int> next_schedule_delay_ms(rtc::nullopt);
  if (repeated) {
    CancelScheduledRegathering();
    next_schedule_delay_ms = delay_ms;
  }
  invoker_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(
          &BasicRegatheringController::RegatherOnFailedNetworksIfDoneGathering,
          this, repeated, next_schedule_delay_ms),
      delay_ms);
}

void BasicRegatheringController::RegatherOnFailedNetworksIfDoneGathering(
    bool repeated,
    rtc::Optional<int> next_schedule_delay_ms) {
  // Only re-gather when the current session is in the CLEARED state (i.e., not
  // running or stopped). It is only possible to enter this state when we gather
  // continually, so there is an implicit check on continual gathering here.
  if (allocator_session_ && allocator_session_->IsCleared()) {
    allocator_session_->RegatherOnFailedNetworks();
  }
  if (repeated) {
    RTC_DCHECK(next_schedule_delay_ms);
    ScheduleRegatheringOnFailedNetworks(next_schedule_delay_ms.value(), true);
  }
}

void BasicRegatheringController::CancelScheduledRegathering() {
  invoker_.Clear();
}

bool BasicRegatheringController::TooManyWeakSelectedCandidatePairs(
    const webrtc::IceTransportStats& stats) const {
  return stats.num_continual_switchings_to_weak_candidate_pairs >
         cricket::kMinNumSwitchingsToWeakCandidatePairsBeforeRegathering;
}

bool BasicRegatheringController::TooLargePingRttOverSelectedCandidatePair(
    const webrtc::IceTransportStats& stats) const {
  return stats.selected_candidate_pair_connectivity_check_rtt_ms >
         cricket::kMinRttMsOverSelectedCandidatePairBeforeRegathering;
}

bool BasicRegatheringController::ShouldRegatherOnAllNetworks(
    const webrtc::IceTransportStats& stats) {
  if (rtc::TimeMillis() <
      last_regathering_ms_ + min_regathering_interval_ms_or_default()) {
    return false;
  }
  if (TooManyWeakSelectedCandidatePairs(stats) ||
      TooLargePingRttOverSelectedCandidatePair(stats)) {
    return true;
  }
  return false;
}

int BasicRegatheringController::SampleRegatherAllNetworksInterval(
    const rtc::IntervalRange& range) {
  return rand_.Rand(range.min(), range.max());
}

}  // namespace webrtc
