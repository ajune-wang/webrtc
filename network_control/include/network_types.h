/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_CONTROL_INCLUDE_NETWORK_TYPES_H_
#define NETWORK_CONTROL_INCLUDE_NETWORK_TYPES_H_
#include <stdint.h>
#include <ostream>
#include <vector>
#include "modules/include/module_common_types.h"
#include "network_control/include/network_message.h"
#include "network_control/include/network_units.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
namespace network {

// Configuration

struct StreamsConfig : public signal::Message<StreamsConfig> {
  // Use this for information about streams that is required for specific
  // adjustments to the algorithms in network controllers. Especially useful
  // for experiments.
  bool requests_alr_probing = false;
  double pacing_factor = 1;
  units::DataRate min_pacing_rate;
  units::DataRate max_padding_rate;
};

struct TargetRateConstraints : public signal::Message<TargetRateConstraints> {
  units::Timestamp at_time;
  units::DataRate starting_rate;
  units::DataRate min_data_rate;
  units::DataRate max_data_rate;
};

// Send side information

struct NetworkAvailability : public signal::Message<NetworkAvailability> {
  units::Timestamp at_time;
  bool network_available = false;
};

struct NetworkRouteChange : public signal::Message<NetworkRouteChange> {
  units::Timestamp at_time;
  // Theese are set here so they can be changed synchronously when network
  // route changes
  TargetRateConstraints constraints;
};

struct SentPacket : public signal::Message<SentPacket> {
  units::Timestamp send_time;
  units::DataSize size;
  PacedPacketInfo pacing_info;
};

struct PacerQueueUpdate : signal::Message<PacerQueueUpdate> {
  network::units::TimeDelta expected_queue_time;
};

// Transport level feedback

struct RemoteBitrateReport : public signal::Message<RemoteBitrateReport> {
  units::Timestamp receive_time;
  units::DataRate bandwidth;
};

struct RoundTripTimeReport : public signal::Message<RoundTripTimeReport> {
  units::Timestamp receive_time;
  units::TimeDelta round_trip_time;
};

struct TransportLossReport : public signal::Message<TransportLossReport> {
  units::Timestamp receive_time;
  units::Timestamp start_time;
  units::Timestamp end_time;
  uint64_t packets_lost_delta = 0;
  uint64_t packets_received_delta = 0;
};

struct OutstandingData : signal::Message<OutstandingData> {
  network::units::DataSize in_flight_data;
};

// Packet level feedback

struct NetworkPacketFeedback {
  rtc::Optional<units::Timestamp> receive_time;
  rtc::Optional<SentPacket> sent_packet;
  NetworkPacketFeedback();
  NetworkPacketFeedback(const NetworkPacketFeedback&);
  ~NetworkPacketFeedback();
};

struct TransportPacketsFeedback
    : public signal::Message<TransportPacketsFeedback> {
  units::Timestamp feedback_time;
  units::DataSize data_in_flight;
  units::DataSize prior_in_flight;
  std::vector<NetworkPacketFeedback> packet_feedbacks;

  std::vector<NetworkPacketFeedback> ReceivedWithHistory();
  std::vector<NetworkPacketFeedback> LostWithHistory();

  TransportPacketsFeedback();
  TransportPacketsFeedback(const TransportPacketsFeedback& other);
  ~TransportPacketsFeedback();
};

// Network estimation

struct NetworkEstimate : public signal::Message<NetworkEstimate> {
  units::Timestamp at_time;
  units::DataRate bandwidth;
  units::TimeDelta round_trip_time;
  units::TimeDelta bwe_period;

  float loss_rate_ratio = 0;
  uint8_t GetLossRatioUint8();

  bool changed = true;
};

// Network control
struct CongestionWindow : public signal::Message<CongestionWindow> {
  bool enabled = true;
  units::DataSize data_window;
};

struct PacerConfig : public signal::Message<PacerConfig> {
  units::Timestamp at_time;
  // Pacer will send at most data_window data over time_window duration
  units::DataSize data_window;
  units::TimeDelta time_window;
  // Pacer will send at least pad_window data over time_window duration
  units::DataSize pad_window;
  units::DataRate data_rate() const { return data_window / time_window; }
};

struct ProbeClusterConfig : signal::Message<ProbeClusterConfig> {
  units::Timestamp time_created;
  units::DataRate target_data_rate;
  units::TimeDelta target_duration;
  uint32_t target_probe_count;
};

struct TargetTransferRate : public signal::Message<TargetTransferRate> {
  units::Timestamp at_time;
  units::DataRate target_rate;
  NetworkEstimate basis_estimate;
};

// Process control
struct ProcessInterval : signal::Message<ProcessInterval> {
  units::Timestamp at_time;
};

::std::ostream& operator<<(::std::ostream& os,
                           const ProbeClusterConfig& config);
::std::ostream& operator<<(::std::ostream& os, const PacerConfig& config);

extern template struct signal::Message<CongestionWindow>;
extern template struct signal::Message<NetworkAvailability>;
extern template struct signal::Message<NetworkEstimate>;
extern template struct signal::Message<NetworkRouteChange>;
extern template struct signal::Message<OutstandingData>;
extern template struct signal::Message<PacerConfig>;
extern template struct signal::Message<ProbeClusterConfig>;
extern template struct signal::Message<PacerQueueUpdate>;
extern template struct signal::Message<ProcessInterval>;
extern template struct signal::Message<RemoteBitrateReport>;
extern template struct signal::Message<RoundTripTimeReport>;
extern template struct signal::Message<SentPacket>;
extern template struct signal::Message<StreamsConfig>;
extern template struct signal::Message<TargetRateConstraints>;
extern template struct signal::Message<TargetTransferRate>;
extern template struct signal::Message<TransportLossReport>;
extern template struct signal::Message<TransportPacketsFeedback>;

}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_TYPES_H_
