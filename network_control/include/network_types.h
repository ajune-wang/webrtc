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

// Configuration

struct StreamsConfig : public webrtc::SignalMessage<StreamsConfig> {
  // Use this for information about streams that is required for specific
  // adjustments to the algorithms in network controllers. Especially useful
  // for experiments.
  bool requests_alr_probing = false;
  double pacing_factor = 1;
  DataRate min_pacing_rate;
  DataRate max_padding_rate;
};

struct TargetRateConstraints
    : public webrtc::SignalMessage<TargetRateConstraints> {
  Timestamp at_time;
  DataRate starting_rate;
  DataRate min_data_rate;
  DataRate max_data_rate;
};

// Send side information

struct NetworkAvailability : public webrtc::SignalMessage<NetworkAvailability> {
  Timestamp at_time;
  bool network_available = false;
};

struct NetworkRouteChange : public webrtc::SignalMessage<NetworkRouteChange> {
  Timestamp at_time;
  // These are set here so they can be changed synchronously when network route
  // changes.
  TargetRateConstraints constraints;
};

struct SentPacket : public webrtc::SignalMessage<SentPacket> {
  Timestamp send_time;
  DataSize size;
  PacedPacketInfo pacing_info;
};

struct PacerQueueUpdate : webrtc::SignalMessage<PacerQueueUpdate> {
  TimeDelta expected_queue_time;
};

// Transport level feedback

struct RemoteBitrateReport : public webrtc::SignalMessage<RemoteBitrateReport> {
  Timestamp receive_time;
  DataRate bandwidth;
};

struct RoundTripTimeReport : public webrtc::SignalMessage<RoundTripTimeReport> {
  Timestamp receive_time;
  TimeDelta round_trip_time;
};

struct TransportLossReport : public webrtc::SignalMessage<TransportLossReport> {
  Timestamp receive_time;
  Timestamp start_time;
  Timestamp end_time;
  uint64_t packets_lost_delta = 0;
  uint64_t packets_received_delta = 0;
};

struct OutstandingData : webrtc::SignalMessage<OutstandingData> {
  DataSize in_flight_data;
};

// Packet level feedback

struct NetworkPacketFeedback {
  rtc::Optional<Timestamp> receive_time;
  rtc::Optional<SentPacket> sent_packet;
  NetworkPacketFeedback();
  NetworkPacketFeedback(const NetworkPacketFeedback&);
  ~NetworkPacketFeedback();
};

struct TransportPacketsFeedback
    : public webrtc::SignalMessage<TransportPacketsFeedback> {
  Timestamp feedback_time;
  DataSize data_in_flight;
  DataSize prior_in_flight;
  std::vector<NetworkPacketFeedback> packet_feedbacks;

  std::vector<NetworkPacketFeedback> ReceivedWithHistory();
  std::vector<NetworkPacketFeedback> LostWithHistory();

  TransportPacketsFeedback();
  TransportPacketsFeedback(const TransportPacketsFeedback& other);
  ~TransportPacketsFeedback();
};

// Network estimation

struct NetworkEstimate : public webrtc::SignalMessage<NetworkEstimate> {
  Timestamp at_time;
  DataRate bandwidth;
  TimeDelta round_trip_time;
  TimeDelta bwe_period;

  float loss_rate_ratio = 0;
  uint8_t GetLossRatioUint8();

  bool changed = true;
};

// Network control
struct CongestionWindow : public webrtc::SignalMessage<CongestionWindow> {
  bool enabled = true;
  DataSize data_window;
};

struct PacerConfig : public webrtc::SignalMessage<PacerConfig> {
  Timestamp at_time;
  // Pacer will send at most data_window data over time_window duration
  DataSize data_window;
  TimeDelta time_window;
  // Pacer will send at least pad_window data over time_window duration
  DataSize pad_window;
  DataRate data_rate() const { return data_window / time_window; }
};

struct ProbeClusterConfig : webrtc::SignalMessage<ProbeClusterConfig> {
  Timestamp time_created;
  DataRate target_data_rate;
  TimeDelta target_duration;
  uint32_t target_probe_count;
};

struct TargetTransferRate : public webrtc::SignalMessage<TargetTransferRate> {
  Timestamp at_time;
  DataRate target_rate;
  NetworkEstimate basis_estimate;
};

// Process control
struct ProcessInterval : webrtc::SignalMessage<ProcessInterval> {
  Timestamp at_time;
};

::std::ostream& operator<<(::std::ostream& os,
                           const ProbeClusterConfig& config);
::std::ostream& operator<<(::std::ostream& os, const PacerConfig& config);

extern template struct webrtc::SignalMessage<CongestionWindow>;
extern template struct webrtc::SignalMessage<NetworkAvailability>;
extern template struct webrtc::SignalMessage<NetworkEstimate>;
extern template struct webrtc::SignalMessage<NetworkRouteChange>;
extern template struct webrtc::SignalMessage<OutstandingData>;
extern template struct webrtc::SignalMessage<PacerConfig>;
extern template struct webrtc::SignalMessage<ProbeClusterConfig>;
extern template struct webrtc::SignalMessage<PacerQueueUpdate>;
extern template struct webrtc::SignalMessage<ProcessInterval>;
extern template struct webrtc::SignalMessage<RemoteBitrateReport>;
extern template struct webrtc::SignalMessage<RoundTripTimeReport>;
extern template struct webrtc::SignalMessage<SentPacket>;
extern template struct webrtc::SignalMessage<StreamsConfig>;
extern template struct webrtc::SignalMessage<TargetRateConstraints>;
extern template struct webrtc::SignalMessage<TargetTransferRate>;
extern template struct webrtc::SignalMessage<TransportLossReport>;
extern template struct webrtc::SignalMessage<TransportPacketsFeedback>;
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_TYPES_H_
