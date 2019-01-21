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

class CrossTrafficSendStrategy {
 public:
  virtual ~CrossTrafficSendStrategy() = default;

  virtual std::vector<rtc::CopyOnWriteBuffer> GetPacketsToSend(
      Timestamp at_time,
      TimeDelta delta) = 0;
  virtual DataRate TrafficRate() const = 0;
};

// Represents cross traffic that is going throw the network. It can be used to
// emulate unexpected network load.
class CrossTraffic {
 public:
  CrossTraffic(Clock* clock,
               EmulatedNetworkReceiverInterface* receiver,
               uint64_t dest_endpoint_id,
               std::unique_ptr<CrossTrafficSendStrategy> send_strategy);
  ~CrossTraffic();

  uint64_t GetDestinationId() const;
  // Triggers sending of dummy packets with size |packet_size| bytes.
  void TriggerPacketBurst(size_t num_packets, size_t packet_size);

  void Process(Timestamp at_time);

  ColumnPrinter StatsPrinter();
  DataRate TrafficRate() const;

 private:
  Timestamp Now();

  Clock* const clock_;
  EmulatedNetworkReceiverInterface* const receiver_;
  const uint64_t dest_endpoint_id_;
  std::unique_ptr<CrossTrafficSendStrategy> send_strategy_;

  Timestamp last_process_time_;
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

class RandomWalkSendStrategy : public CrossTrafficSendStrategy {
 public:
  explicit RandomWalkSendStrategy(RandomWalkConfig config);
  ~RandomWalkSendStrategy() override;

  std::vector<rtc::CopyOnWriteBuffer> GetPacketsToSend(
      Timestamp at_time,
      TimeDelta delta) override;
  DataRate TrafficRate() const override;

 private:
  RandomWalkConfig config_;
  webrtc::Random random_;

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

class PulsedPeaksSendStrategy : public CrossTrafficSendStrategy {
 public:
  explicit PulsedPeaksSendStrategy(PulsedPeaksConfig config);
  ~PulsedPeaksSendStrategy() override;

  std::vector<rtc::CopyOnWriteBuffer> GetPacketsToSend(
      Timestamp at_time,
      TimeDelta delta) override;
  DataRate TrafficRate() const override;

 private:
  PulsedPeaksConfig config_;

  TimeDelta time_since_update_ = TimeDelta::Zero();
  Timestamp last_send_time_ = Timestamp::MinusInfinity();
  double intensity_ = 0;
  DataSize pending_size_ = DataSize::Zero();
};

class IdleSendStrategy : public CrossTrafficSendStrategy {
 public:
  std::vector<rtc::CopyOnWriteBuffer> GetPacketsToSend(
      Timestamp at_time,
      TimeDelta delta) override;
  DataRate TrafficRate() const override;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_CROSS_TRAFFIC_H_
