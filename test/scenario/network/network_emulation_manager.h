/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_NETWORK_EMULATION_MANAGER_H_
#define TEST_SCENARIO_NETWORK_NETWORK_EMULATION_MANAGER_H_

#include <memory>
#include <utility>
#include <vector>

#include "api/test/simulated_network.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread.h"
#include "test/scenario/network/cross_traffic.h"
#include "test/scenario/network/fake_network_socket_server.h"
#include "test/scenario/network/network_emulation.h"
#include "test/scenario/network/time_controller.h"

namespace webrtc {
namespace test {

// NetworkEmulationManager should be deleted after TimeController was stopped.
class NetworkEmulationManager {
 public:
  explicit NetworkEmulationManager(TimeController* time_controller);
  ~NetworkEmulationManager();

  EmulatedNetworkNode* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior,
      size_t packet_overhead = 0);

  // TODO(titovartem) add method without IP address, where manager
  // will provided some unique generated address.
  EndpointNode* CreateEndpoint(rtc::IPAddress ip);

  void CreateRoute(EndpointNode* from,
                   std::vector<EmulatedNetworkNode*> via_nodes,
                   EndpointNode* to);
  void ClearRoute(EndpointNode* from,
                  std::vector<EmulatedNetworkNode*> via_nodes,
                  EndpointNode* to);

  CrossTrafficRoute* CreateCrossTraffic(
      std::vector<EmulatedNetworkNode*> via_nodes);
  RandomWalkCrossTraffic* CreateRandomWalkCrossTraffic(
      CrossTrafficRoute* cross_traffic,
      RandomWalkConfig config);
  RandomWalkCrossTraffic* CreateRandomWalkCrossTraffic(
      CrossTrafficRoute* cross_traffic,
      RandomWalkConfig config,
      Destination destination);
  PulsedPeaksCrossTraffic* CreatePulsedPeaksCrossTraffic(
      CrossTrafficRoute* cross_traffic,
      PulsedPeaksConfig config);
  PulsedPeaksCrossTraffic* CreatePulsedPeaksCrossTraffic(
      CrossTrafficRoute* cross_traffic,
      PulsedPeaksConfig config,
      Destination destination);

  rtc::Thread* CreateNetworkThread(std::vector<EndpointNode*> endpoints);

  void Start();
  void Stop();

 private:
  FakeNetworkSocketServer* CreateSocketServer(
      std::vector<EndpointNode*> endpoints);
  void ProcessNetworkPackets();
  Timestamp Now() const;

  TimeController* time_controller_;
  int next_node_id_;

  // All objects can be added to the manager only when it is idle.
  std::vector<std::unique_ptr<EndpointNode>> endpoints_;
  std::vector<std::unique_ptr<EmulatedNetworkNode>> network_nodes_;
  std::vector<std::unique_ptr<CrossTrafficRoute>> cross_traffics_;
  std::vector<std::unique_ptr<RandomWalkCrossTraffic>> random_cross_traffics_;
  std::vector<std::unique_ptr<PulsedPeaksCrossTraffic>> pulsed_cross_traffics_;
  std::vector<std::unique_ptr<FakeNetworkSocketServer>> socket_servers_;
  std::vector<std::unique_ptr<rtc::Thread>> threads_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_NETWORK_EMULATION_MANAGER_H_
