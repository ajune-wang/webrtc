/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <memory>

#include "absl/memory/memory.h"
#include "api/test/simulated_network.h"
#include "call/simulated_network.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scenario/network/cross_traffic.h"
#include "test/scenario/network/network_emulation.h"
#include "test/scenario/network/network_emulation_manager.h"
#include "test/scenario/network/time_controller.h"

namespace webrtc {
namespace test {
namespace {

class CountingReceiver : public EmulatedNetworkReceiverInterface {
 public:
  void OnPacketReceived(EmulatedIpPacket packet) override {
    packets_count_++;
    total_packets_size_ += packet.size();
  }

  int packets_count() const { return packets_count_; }
  uint64_t total_packets_size() const { return total_packets_size_; }

 private:
  std::atomic<int> packets_count_{0};
  std::atomic<uint64_t> total_packets_size_{0};
};

}  // namespace

TEST(CrossTrafficTest, TriggerPacketBurst) {
  RealTimeController time_controller;
  NetworkEmulationManager network_manager(&time_controller);

  EmulatedNetworkNode* node_a = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* node_b = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));

  CrossTraffic* traffic = network_manager.CreateCrossTraffic({node_a, node_b});

  CountingReceiver counter;
  // TODO(titovartem) think how to specify cross traffic receiver on creation.
  node_b->RemoveReceiver(traffic->GetDestinationId());
  node_b->SetReceiver(traffic->GetDestinationId(), &counter);

  traffic->TriggerPacketBurst(100, 1000);

  network_manager.Start();
  rtc::Event event;
  event.Wait(1000);
  network_manager.Stop();

  ASSERT_EQ(counter.packets_count(), 100);
  ASSERT_EQ(counter.total_packets_size(), 100 * 1000ul);
}

TEST(CrossTrafficTest, PulsedPeaksSendStrategy) {
  RealTimeController time_controller;
  NetworkEmulationManager network_manager(&time_controller);

  EmulatedNetworkNode* node_a = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* node_b = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));

  PulsedPeaksConfig cfg;
  cfg.peak_rate = DataRate::kbps(819.2);  // Send 100kB of data per second
  cfg.min_packet_size = DataSize::bytes(1);
  cfg.min_packet_interval = TimeDelta::ms(100);
  cfg.send_duration = TimeDelta::ms(500);
  cfg.hold_duration = TimeDelta::ms(250);
  CrossTraffic* traffic = network_manager.CreateCrossTraffic({node_a, node_b});
  network_manager.CreatePulsedPeaksCrossTraffic(traffic, cfg);

  CountingReceiver counter;
  // TODO(titovartem) think how to specify cross traffic receiver on creation.
  node_b->RemoveReceiver(traffic->GetDestinationId());
  node_b->SetReceiver(traffic->GetDestinationId(), &counter);

  network_manager.Start();
  rtc::Event event;
  event.Wait(1000);
  network_manager.Stop();

  RTC_LOG(INFO) << counter.packets_count() << " packets; "
                << counter.total_packets_size() << " bytes";
  ASSERT_NEAR(counter.total_packets_size(), 50 * 1024ul, 200);
}

}  // namespace test
}  // namespace webrtc
