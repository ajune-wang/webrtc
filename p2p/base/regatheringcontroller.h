/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_REGATHERINGCONTROLLER_H_
#define P2P_BASE_REGATHERINGCONTROLLER_H_

#include "p2p/base/icetransportstats.h"
#include "p2p/base/portallocator.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/random.h"
#include "rtc_base/thread.h"

namespace webrtc {

// All methods of BasicRegatheringController should be called on the same
// thread as the one passed to the constructor, and this thread should be the
// same one where the port allocator runs, which is also identical to the
// network thread in P2PTransportChannel.
class BasicRegatheringController {
 public:
  BasicRegatheringController() = delete;
  explicit BasicRegatheringController(rtc::Thread* thread);
  ~BasicRegatheringController();
  // Schedules delayed regathering of local candidates on all networks, where
  // the delay in milliseconds is randomly sampled from the given range. The
  // schedule can be set repeated and the delay of each repetition is
  // independently sampled from the same range. When repeated regathering is
  // scheduled, all previous schedules are canceled.
  void ScheduleRegatheringOnAllNetworks(rtc::IntervalRange delay_ms_range,
                                        bool repeated);
  // Schedules delayed regathering of local candidates on failed networks. The
  // schedule can be set repeated and each repetition is separated by the same
  // delay. When repeated regathering is scheduled, all previous schedules are
  // canceled.
  void ScheduleRegatheringOnFailedNetworks(int delay_ms, bool repeated);
  // Cancels scheduled regathering on all networks.
  void CancelScheduledRegathering();
  bool ShouldRegatherOnAllNetworks(const webrtc::IceTransportStats& stats);
  void set_allocator_session(cricket::PortAllocatorSession* allocator_session) {
    allocator_session_ = allocator_session;
  }

  int min_regathering_interval_ms_or_default() const {
    return min_regathering_interval_ms_.value_or(
        cricket::kMinRegatheringIntervalMs);
  }

 private:
  rtc::Thread* thread() const { return thread_; }
  // The following two methods perform the actual regathering, if the recent
  // port allocator session has done the initial gathering.
  void RegatherOnAllNetworksIfDoneGathering(
      bool repeated,
      rtc::Optional<rtc::IntervalRange> next_schedule_delay_ms_range);
  void RegatherOnFailedNetworksIfDoneGathering(
      bool repeated,
      rtc::Optional<int> next_schedule_delay_ms);
  // Samples a delay from the uniform distribution in the given range.
  int SampleRegatherAllNetworksInterval(const rtc::IntervalRange& range);

  bool TooManyWeakSelectedCandidatePairs(
      const webrtc::IceTransportStats& stats) const;
  bool TooLargePingRttOverSelectedCandidatePair(
      const webrtc::IceTransportStats& stats) const;

  cricket::PortAllocatorSession* allocator_session_ = nullptr;
  rtc::Thread* thread_;
  rtc::AsyncInvoker invoker_;
  // Used to generate random intervals for regather_all_networks_interval_range.
  Random rand_;
  //
  rtc::Optional<int> min_regathering_interval_ms_;
  // Last time in milliseconds a round of regathering is done.
  int last_regathering_ms_ = 0;
};

}  // namespace webrtc

#endif  // P2P_BASE_REGATHERINGCONTROLLER_H_
