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

#include "p2p/base/icetransportinternal.h"
#include "p2p/base/icetransportstats.h"
#include "p2p/base/portallocator.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/random.h"
#include "rtc_base/thread.h"

namespace webrtc {

// Controls regathering of candidates for the ICE transport passed into it,
// reacting to signals like SignalWritableState, SignalNetworkRouteChange, etc.,
// using methods like GetStats to get additional information, and calling
// methods like RegatherOnAllNetworks on the PortAllocatorSession when
// regathering is desired.
//
// TODO(qingsi): Add the description of behavior when autonomous regathering is
// implemented.
//
// "Regathering" is defined as gathering additional candidates within a single
// ICE generation (or in other words, PortAllocatorSession), and is possible
// when "continual gathering" is enabled. This may allow connectivity to be
// maintained and/or restored without a full ICE restart.
//
// Regathering will only begin after PortAllocationSession is set via
// SetAllocatorSession. This should be called any time the "active"
// PortAllocatorSession is changed (in other words, when an ICE restart occurs),
// so that candidates are gathered for the "current" ICE generation.
//
// All methods of BasicRegatheringController should be called on the same
// thread as the one passed to the constructor, and this thread should be the
// same one where PortAllocatorSession runs, which is also identical to the
// network thread of the ICE transport, as given by
// P2PTransportChannel::thread().
class BasicRegatheringController : public sigslot::has_slots<> {
 public:
  struct Config {
    Config(const rtc::Optional<rtc::IntervalRange>&
               regather_on_all_networks_interval_range,
           int regather_on_failed_networks_interval);
    Config(const Config& other);
    ~Config();
    Config& operator=(const Config& other);
    rtc::Optional<rtc::IntervalRange> regather_on_all_networks_interval_range;
    int regather_on_failed_networks_interval;
  };

  BasicRegatheringController() = delete;
  BasicRegatheringController(const Config& config,
                             cricket::IceTransportInternal* ice_transport,
                             rtc::Thread* thread);
  ~BasicRegatheringController() override;
  // TODO(qingsi): Remove this method after implementing a new signal in
  // P2PTransportChannel and reacting to that signal for the initial schedules
  // of regathering.
  void Start();
  void SetAllocatorSession(cricket::PortAllocatorSession* allocator_session);
  // Setting a different config of the regathering interval range on all
  // networks cancels and reschedules the recurring schedules, if any, of
  // regathering on all networks. The same applies to the change of the
  // regathering interval on the failed networks. This rescheduling behavior is
  // seperately defined for the two config parameters.
  void SetConfig(const Config& config);

 private:
  // TODO(qingsi): Implement the following methods and use methods from the ICE
  // transport like GetStats to get additional information for the decision
  // making in regathering.
  void OnIceTransportStateChanged(cricket::IceTransportInternal*);
  void OnIceTransportWritableState(rtc::PacketTransportInternal*);
  void OnIceTransportReceivingState(rtc::PacketTransportInternal*);
  void OnIceTransportNetworkRouteChanged(rtc::Optional<rtc::NetworkRoute>);
  void OnCandidateFilterChanged(uint32_t new_filter);

  void MaybeRegatherOnAllNetworks();
  // Schedules delayed and repeated regathering of local candidates on all
  // networks, where the delay in milliseconds is randomly sampled from the
  // range in the config. The delay of each repetition is independently sampled
  // from the same range. When scheduled, all previous schedules are canceled.
  void ScheduleRecurringRegatheringOnAllNetworks();
  // One-time schedule with a range differnet from the config.
  void ScheduleOneTimeRegatheringOnAllNetworks(const rtc::IntervalRange& range);
  // Schedules delayed and repeated regathering of local candidates on failed
  // networks, where the delay ini milliseconds is given by the config. Each
  // repetition is separated by the same delay. When scheduled, all previous
  // schedules are canceled.
  void ScheduleRecurringRegatheringOnFailedNetworks();
  // Cancels regathering scheduled by ScheduleRecurringRegatheringOnAllNetworks.
  void CancelScheduledRecurringRegatheringOnAllNetworks();
  // Cancels regathering scheduled by
  // ScheduleRecurringRegatheringOnFailedNetworks.
  void CancelScheduledRecurringRegatheringOnFailedNetworks();

  rtc::Thread* thread() const { return thread_; }
  // The following two methods perform the actual regathering, if the recent
  // port allocator session has done the initial gathering.
  void RegatherOnAllNetworksIfDoneGathering(bool repeated);
  void RegatherOnFailedNetworksIfDoneGathering(bool repeated);
  // Samples a delay from the uniform distribution in the given range.
  int SampleRegatherAllNetworksInterval(const rtc::IntervalRange& range);

  bool TooManyWeakSelectedCandidatePairs(
      const webrtc::IceTransportStats& stats) const;
  bool TooLargePingRttOverSelectedCandidatePair(
      const webrtc::IceTransportStats& stats) const;
  bool HadSelectedCandidatePair(const webrtc::IceTransportStats& stats) const;
  bool HasActiveCandidatePair(const webrtc::IceTransportStats& stats) const;
  bool HasWritableCandidatePair(const webrtc::IceTransportStats& stats) const;

  bool ShouldRegatherOnAllNetworks(const IceTransportStats& stats);

  int min_regathering_interval_ms_or_default() const {
    return min_regathering_interval_ms_.value_or(
        cricket::kMinRegatheringIntervalMs);
  }

  Config config_;
  cricket::IceTransportInternal* ice_transport_;
  cricket::PortAllocatorSession* allocator_session_ = nullptr;
  bool has_recurring_schedule_on_all_networks_ = false;
  bool has_recurring_schedule_on_failed_networks_ = false;
  rtc::Thread* thread_;
  rtc::AsyncInvoker invoker_for_all_networks_;
  rtc::AsyncInvoker invoker_for_failed_networks_;
  rtc::AsyncInvoker invoker_for_one_time_regathering_on_all_networks_;
  // Used to generate random intervals for regather_all_networks_interval_range.
  Random rand_;
  //
  rtc::Optional<int> min_regathering_interval_ms_;
  // Last time in milliseconds a round of regathering is done.
  rtc::Optional<int> last_regathering_ms_on_all_networks_;
  rtc::Optional<int> last_time_ms_writable_;
  rtc::Optional<uint32_t> prev_candidate_filter_;
};

}  // namespace webrtc

#endif  // P2P_BASE_REGATHERINGCONTROLLER_H_
