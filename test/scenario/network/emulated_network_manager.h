/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_EMULATED_NETWORK_MANAGER_H_
#define TEST_SCENARIO_NETWORK_EMULATED_NETWORK_MANAGER_H_

#include <vector>

#include "rtc_base/critical_section.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/network.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"
#include "test/scenario/network/network_emulation.h"

namespace webrtc {
namespace test {

class EmulatedNetworkManager : public rtc::NetworkManagerBase,
                               public rtc::MessageHandler,
                               public sigslot::has_slots<> {
 public:
  explicit EmulatedNetworkManager(std::vector<EmulatedEndpoint*> endpoints);

  EmulatedEndpoint* GetEndpointNode(const rtc::IPAddress& ip) const;
  void EnableEndpoint(EmulatedEndpoint* endpoint);
  void DisableEndpoint(EmulatedEndpoint* endpoint);

  // MessageHandler interface
  void OnMessage(rtc::Message* msg) override;

  // NetworkManager interface. All these methods are supposed to be called from
  // the same thread.
  void StartUpdating() override;
  void StopUpdating() override;

 private:
  bool HasEndpoint(EmulatedEndpoint* endpoint)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(endpoints_lock_);
  void MaybePostUpdateNetworks();
  void UpdateNetworksOnce();

  rtc::ThreadChecker thread_checker_;

  rtc::CriticalSection thread_lock_;
  rtc::Thread* thread_ RTC_GUARDED_BY(thread_lock_);
  bool sent_first_update_;
  int start_count_;

  rtc::CriticalSection endpoints_lock_;
  std::vector<EmulatedEndpoint*> endpoints_ RTC_GUARDED_BY(endpoints_lock_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_EMULATED_NETWORK_MANAGER_H_
