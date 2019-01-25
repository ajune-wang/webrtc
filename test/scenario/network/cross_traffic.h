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

// Represents the endpoint for cross traffic that is going through the network.
// It can be used to emulate unexpected network load.
class TrafficRoute {
 public:
  TrafficRoute(Clock* clock,
               EmulatedNetworkReceiverInterface* receiver,
               EndpointNode* endpoint);
  ~TrafficRoute();

  // Triggers sending of dummy packets with size |packet_size| bytes.
  void TriggerPacketBurst(size_t num_packets, size_t packet_size);
  // Sends a packet over the nodes and runs |action| when it has been delivered.
  void NetworkDelayedAction(size_t packet_size, std::function<void()> action);

  void SendPacket(rtc::CopyOnWriteBuffer data);

 private:
  void SendPacket(rtc::CopyOnWriteBuffer data, uint16_t dest_port);

  Clock* const clock_;
  EmulatedNetworkReceiverInterface* const receiver_;
  EndpointNode* const endpoint_;

  uint16_t null_receiver_port_;
  std::unique_ptr<EmulatedNetworkReceiverInterface> null_receiver_;
  std::vector<std::unique_ptr<EmulatedNetworkReceiverInterface>> actions_;
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
  RandomWalkCrossTraffic(RandomWalkConfig config, TrafficRoute* cross_traffic);
  ~RandomWalkCrossTraffic();

  void Process(Timestamp at_time);
  DataRate TrafficRate() const;
  ColumnPrinter StatsPrinter();

 private:
  void UpdatePendingSize(Timestamp at_time, TimeDelta delta);

  RandomWalkConfig config_;
  TrafficRoute* const cross_traffic_;
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
                          TrafficRoute* cross_traffic);
  ~PulsedPeaksCrossTraffic();

  void Process(Timestamp at_time);
  DataRate TrafficRate() const;
  ColumnPrinter StatsPrinter();

 private:
  void UpdatePendingSize(Timestamp at_time, TimeDelta delta);

  PulsedPeaksConfig config_;
  TrafficRoute* const cross_traffic_;

  Timestamp last_process_time_ = Timestamp::MinusInfinity();
  TimeDelta time_since_update_ = TimeDelta::Zero();
  Timestamp last_send_time_ = Timestamp::MinusInfinity();
  double intensity_ = 0;
  DataSize pending_size_ = DataSize::Zero();
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_CROSS_TRAFFIC_H_
