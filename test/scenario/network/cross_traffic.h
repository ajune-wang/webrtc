/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_CROSS_TRAFFIC_H_
#define TEST_SCENARIO_NETWORK_CROSS_TRAFFIC_H_

#include <memory>
#include <vector>

#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/random.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network/network_emulation.h"

namespace webrtc {
namespace test {

// Represents a destination attached to the CrossTrafficRoute. Don't create
// it by yourself. Use CrossTrafficRoute::GetDefaultDestination() or
// CrossTrafficRoute::RegisterDestination(...) to create it.
struct Destination {
  // Some id, that represents a destination within CrossTrafficRoute with which
  // destination was created. Do not make any assumptions about its meaning,
  // because it is private to CrossTrafficRoute implementation.
  uint16_t id;
};

// Represents the endpoint for cross traffic that is going through the network.
// It can be used to emulate unexpected network load.
class CrossTrafficRoute {
 public:
  CrossTrafficRoute(Clock* clock,
                    EmulatedNetworkReceiverInterface* receiver,
                    EndpointNode* endpoint);
  ~CrossTrafficRoute();

  // Triggers sending of dummy packets with size |packet_size| bytes.
  void TriggerPacketBurst(size_t num_packets, size_t packet_size);
  void TriggerPacketBurst(size_t num_packets,
                          size_t packet_size,
                          const Destination& destination);
  // Sends a packet over the nodes and runs |action| when it has been delivered.
  void NetworkDelayedAction(size_t packet_size, std::function<void()> action);

  Destination GetDefaultDestination() const;
  Destination RegisterDestination(
      std::unique_ptr<EmulatedNetworkReceiverInterface> receiver);
  void RemoveDestination(const Destination& destination);
  void SendPacket(rtc::SocketAddress source_addr,
                  rtc::CopyOnWriteBuffer data,
                  const Destination& destination);
  void SendPacket(rtc::SocketAddress source_addr,
                  rtc::CopyOnWriteBuffer data,
                  Timestamp at_time,
                  const Destination& destination);

 private:
  Clock* const clock_;
  EmulatedNetworkReceiverInterface* const receiver_;
  EndpointNode* const endpoint_;

  uint16_t null_receiver_port_;
  std::unique_ptr<EmulatedNetworkReceiverInterface> null_receiver_;
  std::vector<std::unique_ptr<EmulatedNetworkReceiverInterface>> actions_;
  std::vector<std::unique_ptr<EmulatedNetworkReceiverInterface>>
      external_receivers_;
};

struct RandomWalkConfig {
  int random_seed = 1;
  DataRate peak_rate = DataRate::kbps(100);
  DataSize min_packet_size = DataSize::bytes(200);
  TimeDelta min_packet_interval = TimeDelta::ms(1);
  TimeDelta update_interval = TimeDelta::ms(200);
  double variance = 0.6;
  double bias = -0.1;
};

class RandomWalkCrossTraffic {
 public:
  RandomWalkCrossTraffic(RandomWalkConfig config,
                         CrossTrafficRoute* cross_traffic,
                         Destination destination);
  ~RandomWalkCrossTraffic();

  void Process(Timestamp at_time);
  DataRate TrafficRate() const;
  ColumnPrinter StatsPrinter();

 private:
  void UpdatePendingSize(Timestamp at_time, TimeDelta delta);

  RandomWalkConfig config_;
  CrossTrafficRoute* const cross_traffic_;
  const Destination destination_;
  webrtc::Random random_;

  Timestamp last_process_time_ = Timestamp::MinusInfinity();
  TimeDelta time_since_update_ = TimeDelta::Zero();
  Timestamp last_send_time_ = Timestamp::MinusInfinity();
  double intensity_ = 0;
  DataSize pending_size_ = DataSize::Zero();
};

struct PulsedPeaksConfig {
  DataRate peak_rate = DataRate::kbps(100);
  DataSize min_packet_size = DataSize::bytes(200);
  TimeDelta min_packet_interval = TimeDelta::ms(1);
  TimeDelta send_duration = TimeDelta::ms(100);
  TimeDelta hold_duration = TimeDelta::ms(2000);
};

class PulsedPeaksCrossTraffic {
 public:
  PulsedPeaksCrossTraffic(PulsedPeaksConfig config,
                          CrossTrafficRoute* cross_traffic,
                          Destination destination);
  ~PulsedPeaksCrossTraffic();

  void Process(Timestamp at_time);
  DataRate TrafficRate() const;
  ColumnPrinter StatsPrinter();

 private:
  void UpdatePendingSize(Timestamp at_time, TimeDelta delta);

  PulsedPeaksConfig config_;
  CrossTrafficRoute* const cross_traffic_;
  const Destination destination_;

  Timestamp last_process_time_ = Timestamp::MinusInfinity();
  TimeDelta time_since_update_ = TimeDelta::Zero();
  Timestamp last_send_time_ = Timestamp::MinusInfinity();
  double intensity_ = 0;
  DataSize pending_size_ = DataSize::Zero();
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_CROSS_TRAFFIC_H_
