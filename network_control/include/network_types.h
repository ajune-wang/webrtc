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
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "network_control/include/network_message.h"
#include "network_control/include/network_units.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
namespace network {

struct LossRate {
  float loss_ratio = 0;
  uint8_t GetLossRatioUint8();
};

struct SentPacket : public signal::Message<SentPacket> {
  units::Timestamp send_time;
  units::DataSize size;
  PacedPacketInfo pacing_info;
};

struct StreamsConfig : public signal::Message<StreamsConfig> {
  // Use this for information about streams that is required for specific
  // adjustments to the algorithms in network controllers. Especially useful
  // for experiments.
  //  bool has_screenshare = false;
  //  bool has_audio = false;
  //  bool supports_transport_feedback = false;
  bool requests_alr_probing = false;
  double pacing_factor = 1;
  units::DataRate min_pacing_rate;
  units::DataRate max_padding_rate;
};

struct SimplePacketFeedback {
  rtc::Optional<units::Timestamp> receive_time;
  rtc::Optional<SentPacket> sent_packet;
  SimplePacketFeedback();
  SimplePacketFeedback(const SimplePacketFeedback&);
  ~SimplePacketFeedback();

  static SimplePacketFeedback FromRtpPacketFeedback(
      const webrtc::PacketFeedback&);

 private:
  explicit SimplePacketFeedback(const webrtc::PacketFeedback&);
};

struct TransportPacketsFeedback
    : public signal::Message<TransportPacketsFeedback> {
  units::Timestamp feedback_time;
  std::vector<SimplePacketFeedback> packet_feedbacks;

  static TransportPacketsFeedback FromRtpFeedbackVector(
      const std::vector<PacketFeedback>&,
      int64_t creation_time_ms);
  static TransportPacketsFeedback FromRtpFeedbackVector(
      const std::vector<PacketFeedback>&);

  TransportPacketsFeedback();
  TransportPacketsFeedback(const TransportPacketsFeedback& other);
  ~TransportPacketsFeedback();

 private:
  TransportPacketsFeedback(const std::vector<PacketFeedback>&, int64_t);
};

struct TransportLossReport : public signal::Message<TransportLossReport> {
  units::Timestamp receive_time;
  units::Timestamp start_time;
  units::Timestamp end_time;
  uint64_t packets_lost_delta = 0;
  uint64_t packets_received_delta = 0;
};

struct RoundTripTimeReport : public signal::Message<RoundTripTimeReport> {
  units::Timestamp receive_time;
  units::TimeDelta round_trip_time;
};

struct RemoteBitrateReport : public signal::Message<RemoteBitrateReport> {
  units::Timestamp receive_time;
  units::DataRate bandwidth;
};

struct TargetRateConstraints : public signal::Message<TargetRateConstraints> {
  units::DataRate starting_rate;
  units::DataRate min_data_rate;
  units::DataRate max_data_rate;
};

struct NetworkAvailability : public signal::Message<NetworkAvailability> {
  bool network_available = false;
};

struct NetworkRouteChange : public signal::Message<NetworkRouteChange> {
  // Theese are set here so they can be changed synchronously on network
  // route changes
  TargetRateConstraints constraints;
};

struct NetworkEstimate : public signal::Message<NetworkEstimate> {
  units::Timestamp at_time;
  units::DataRate bandwidth;
  units::TimeDelta round_trip_time;

  LossRate loss_rate;
  bool changed = true;
};

struct TargetTransferRate : public signal::Message<TargetTransferRate> {
  units::Timestamp at_time;
  units::DataRate target_rate;
  NetworkEstimate basis_estimate;
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

struct PacerState : signal::Message<PacerState> {
  bool paused;
};

struct CongestionWindow : public signal::Message<CongestionWindow> {
  bool enabled = true;
  units::DataSize data_window;
};

struct ProbeClusterConfig : signal::Message<ProbeClusterConfig> {
  units::Timestamp time_created;
  units::DataRate target_data_rate;
  units::TimeDelta target_duration;
  uint32_t target_probe_count;
};

struct ProcessInterval : signal::Message<ProcessInterval> {
  units::Timestamp at_time;
  units::TimeDelta elapsed_time;
};

struct Invalidation : signal::Message<Invalidation> {
  int __dummy;
};

::std::ostream& operator<<(::std::ostream& os,
                           const ProbeClusterConfig& config);
::std::ostream& operator<<(::std::ostream& os, const PacerConfig& config);

extern template struct signal::Message<SentPacket>;
extern template struct signal::Message<StreamsConfig>;
extern template struct signal::Message<TransportPacketsFeedback>;
extern template struct signal::Message<TransportLossReport>;
extern template struct signal::Message<RoundTripTimeReport>;
extern template struct signal::Message<RemoteBitrateReport>;
extern template struct signal::Message<TargetRateConstraints>;
extern template struct signal::Message<NetworkAvailability>;
extern template struct signal::Message<NetworkRouteChange>;
extern template struct signal::Message<NetworkEstimate>;
extern template struct signal::Message<TargetTransferRate>;
extern template struct signal::Message<PacerConfig>;
extern template struct signal::Message<PacerState>;
extern template struct signal::Message<CongestionWindow>;
extern template struct signal::Message<ProbeClusterConfig>;
extern template struct signal::Message<ProcessInterval>;
extern template struct signal::Message<Invalidation>;

}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_TYPES_H_
