/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_EMULATED_NETWORK_MANAGER_H_
#define TEST_NETWORK_EMULATED_NETWORK_MANAGER_H_

#include <functional>
#include <memory>
#include <vector>

#include "api/sequence_checker.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/time_controller.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "test/network/network_emulation.h"

namespace webrtc {
namespace test {

// Framework assumes that rtc::NetworkManager is called from network thread.
class EmulatedNetworkManager : public EmulatedNetworkManagerInterface {
 public:
  EmulatedNetworkManager(TimeController* time_controller,
                         TaskQueueForTest* task_queue,
                         EndpointsContainer* endpoints_container);
  ~EmulatedNetworkManager() override;

  void EnableEndpoint(EmulatedEndpointImpl* endpoint);
  void DisableEndpoint(EmulatedEndpointImpl* endpoint);

  // EmulatedNetworkManagerInterface API
  rtc::Thread* network_thread() override { return network_thread_.get(); }
  rtc::NetworkManager* network_manager() override {
    RTC_DCHECK(network_manager_);
    return network_manager_.get();
  }
  rtc::PacketSocketFactory* packet_socket_factory() override {
    RTC_DCHECK(packet_socket_factory_);
    return packet_socket_factory_.get();
  }
  std::unique_ptr<rtc::NetworkManager> FetchNetworkManager() override {
    RTC_DCHECK(network_manager_);
    return std::move(network_manager_);
  }
  std::unique_ptr<rtc::PacketSocketFactory> FetchPacketSocketFactory()
      override {
    RTC_DCHECK(packet_socket_factory_);
    return std::move(packet_socket_factory_);
  }
  std::vector<EmulatedEndpoint*> endpoints() const override {
    return endpoints_container_->GetEndpoints();
  }
  void GetStats(
      std::function<void(EmulatedNetworkStats)> stats_callback) const override;

 private:
  class RtcNetworkManager;
  void UpdateNetworksOnce();

  TaskQueueForTest* const task_queue_;
  const EndpointsContainer* const endpoints_container_;
  // The `network_thread_` must outlive `packet_socket_factory_`, because they
  // both refer to a socket server that is owned by `network_thread_`. Both
  // pointers are assigned only in the constructor, but the way they are
  // initialized unfortunately doesn't work with const std::unique_ptr<...>.
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::PacketSocketFactory> packet_socket_factory_;
  std::unique_ptr<rtc::NetworkManager> network_manager_;
  RtcNetworkManager* network_manager_ptr_ = nullptr;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_NETWORK_EMULATED_NETWORK_MANAGER_H_
