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

#include "api/optional.h"
#include "p2p/base/p2pconstants.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/messagehandler.h"
#include "rtc_base/random.h"
#include "rtc_base/sigslot.h"

namespace cricket {
class P2PTransportChannel;
}  // namespace cricket

namespace rtc {
class Thread;
}  // namespace rtc

namespace webrtc {

class RegatheringControllerInterface {
 public:
  RegatheringControllerInterface() = default;
  virtual ~RegatheringControllerInterface() = default;
  virtual void ScheduleRegatheringOnAllNetworks(int delay_ms,
                                                bool repeated) = 0;
  virtual void ScheduleRegatheringOnFailedNetworks(int delay_ms,
                                                   bool repeated) = 0;
  virtual void CancelScheduledRegathering() = 0;
  virtual bool ShouldRegatherOnAllNetworks() = 0;
  virtual int regather_on_all_networks_interval() = 0;
  virtual int regather_on_failed_networks_interval() = 0;
};

// All methods of BasicRegatheringController should be called on the same
// thread.
class BasicRegatheringController : public RegatheringControllerInterface {
 public:
  BasicRegatheringController() = delete;
  explicit BasicRegatheringController(
      cricket::P2PTransportChannel* ice_transport);
  ~BasicRegatheringController() override;
  void ScheduleRegatheringOnAllNetworks(int delay_ms, bool repeated) override;
  void ScheduleRegatheringOnFailedNetworks(int delay_ms,
                                           bool repeated) override;
  void CancelScheduledRegathering() override;
  bool ShouldRegatherOnAllNetworks() override;
  int regather_on_all_networks_interval() override;
  int regather_on_failed_networks_interval() override;

  rtc::Thread* thread() const { return thread_; }

  int min_regathering_interval_ms_or_default() const {
    return min_regathering_interval_ms_.value_or(
        cricket::kMinRegatheringIntervalMs);
  }

 private:
  // Samples a delay from the uniform distribution defined by the
  // regather_on_all_networks_interval ICE configuration pair.
  void RegatherOnAllNetworks(bool repeated,
                             rtc::Optional<int> next_schedule_delay_ms);
  void RegatherOnFailedNetworks(bool repeated,
                                rtc::Optional<int> next_schedule_delay_ms);
  int SampleRegatherAllNetworksInterval();

  cricket::P2PTransportChannel* ice_transport_;
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
