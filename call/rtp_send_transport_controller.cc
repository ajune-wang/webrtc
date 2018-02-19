/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <utility>

#include "call/rtp_send_transport_controller.h"
#include "rtc_base/logging.h"

namespace webrtc {

RtpSendTransportController::RtpSendTransportController(
    Clock* clock,
    webrtc::RtcEventLog* event_log,
    const BitrateConfig& bitrate_config)
    : pacer_(clock, &packet_router_, event_log),
      send_side_cc_(clock, nullptr /* observer */, event_log, &pacer_),
      bitrate_configurator_(bitrate_config) {
  send_side_cc_.SignalNetworkState(kNetworkDown);
  send_side_cc_.SetBweBitrates(bitrate_config.min_bitrate_bps,
                               bitrate_config.start_bitrate_bps,
                               bitrate_config.max_bitrate_bps);
}
RtpSendTransportController::~RtpSendTransportController() = default;

PacketRouter* RtpSendTransportController::packet_router() {
  return &packet_router_;
}

TransportFeedbackObserver*
RtpSendTransportController::transport_feedback_observer() {
  return &send_side_cc_;
}

RtpPacketSender* RtpSendTransportController::packet_sender() {
  return &pacer_;
}

const RtpKeepAliveConfig& RtpSendTransportController::keepalive_config() const {
  return keepalive_;
}

void RtpSendTransportController::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps) {
  pacer_.SetSendBitrateLimits(min_send_bitrate_bps, max_padding_bitrate_bps);
}

void RtpSendTransportController::SetKeepAliveConfig(
    const RtpKeepAliveConfig& config) {
  keepalive_ = config;
}
Module* RtpSendTransportController::GetPacerModule() {
  return &pacer_;
}
void RtpSendTransportController::SetPacingFactor(float pacing_factor) {
  pacer_.SetPacingFactor(pacing_factor);
}
void RtpSendTransportController::SetQueueTimeLimit(int limit_ms) {
  pacer_.SetQueueTimeLimit(limit_ms);
}
Module* RtpSendTransportController::GetModule() {
  return &send_side_cc_;
}
CallStatsObserver* RtpSendTransportController::GetCallStatsObserver() {
  return &send_side_cc_;
}
void RtpSendTransportController::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  send_side_cc_.RegisterPacketFeedbackObserver(observer);
}
void RtpSendTransportController::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  send_side_cc_.DeRegisterPacketFeedbackObserver(observer);
}
void RtpSendTransportController::RegisterNetworkObserver(
    NetworkChangedObserver* observer) {
  send_side_cc_.RegisterNetworkObserver(observer);
}
void RtpSendTransportController::DeRegisterNetworkObserver(
    NetworkChangedObserver* observer) {
  send_side_cc_.DeRegisterNetworkObserver(observer);
}
void RtpSendTransportController::OnNetworkRouteChanged(
    const std::string& transport_name,
    const rtc::NetworkRoute& network_route) {
  // Check if the network route is connected.
  if (!network_route.connected) {
    RTC_LOG(LS_INFO) << "Transport " << transport_name << " is disconnected";
    // TODO(honghaiz): Perhaps handle this in SignalChannelNetworkState and
    // consider merging these two methods.
    return;
  }

  // Check whether the network route has changed on each transport.
  auto result =
      network_routes_.insert(std::make_pair(transport_name, network_route));
  auto kv = result.first;
  bool inserted = result.second;
  if (inserted) {
    // No need to reset BWE if this is the first time the network connects.
    return;
  }
  if (kv->second != network_route) {
    kv->second = network_route;
    BitrateConfig bitrate_config = bitrate_configurator_.GetConfig();
    RTC_LOG(LS_INFO) << "Network route changed on transport " << transport_name
                     << ": new local network id "
                     << network_route.local_network_id
                     << " new remote network id "
                     << network_route.remote_network_id
                     << " Reset bitrates to min: "
                     << bitrate_config.min_bitrate_bps
                     << " bps, start: " << bitrate_config.start_bitrate_bps
                     << " bps,  max: " << bitrate_config.max_bitrate_bps
                     << " bps.";
    RTC_DCHECK_GT(bitrate_config.start_bitrate_bps, 0);
    send_side_cc_.OnNetworkRouteChanged(
        network_route, bitrate_config.start_bitrate_bps,
        bitrate_config.min_bitrate_bps, bitrate_config.max_bitrate_bps);
  }
}
void RtpSendTransportController::OnNetworkAvailability(bool network_available) {
  send_side_cc_.SignalNetworkState(network_available ? kNetworkUp
                                                     : kNetworkDown);
}
void RtpSendTransportController::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  send_side_cc_.SetTransportOverhead(transport_overhead_bytes_per_packet);
}
RtcpBandwidthObserver* RtpSendTransportController::GetBandwidthObserver() {
  return send_side_cc_.GetBandwidthObserver();
}
bool RtpSendTransportController::AvailableBandwidth(uint32_t* bandwidth) const {
  return send_side_cc_.AvailableBandwidth(bandwidth);
}
int64_t RtpSendTransportController::GetPacerQueuingDelayMs() const {
  return send_side_cc_.GetPacerQueuingDelayMs();
}
int64_t RtpSendTransportController::GetFirstPacketTimeMs() const {
  return send_side_cc_.GetFirstPacketTimeMs();
}
RateLimiter* RtpSendTransportController::GetRetransmissionRateLimiter() {
  return send_side_cc_.GetRetransmissionRateLimiter();
}
void RtpSendTransportController::EnablePeriodicAlrProbing(bool enable) {
  send_side_cc_.EnablePeriodicAlrProbing(enable);
}
void RtpSendTransportController::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  send_side_cc_.OnSentPacket(sent_packet);
}

void RtpSendTransportController::SetBitrateConfig(
    const BitrateConfig& bitrate_config) {
  rtc::Optional<BitrateConfig> config =
      bitrate_configurator_.UpdateBitrateConfig(bitrate_config);
  if (config.has_value()) {
    send_side_cc_.SetBweBitrates(config->min_bitrate_bps,
                                 config->start_bitrate_bps,
                                 config->max_bitrate_bps);
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpSendTransportController.SetBitrateConfig: "
        << "nothing to update";
  }
}

void RtpSendTransportController::SetBitrateConfigMask(
    const BitrateConfigMask& bitrate_mask) {
  rtc::Optional<BitrateConfig> config =
      bitrate_configurator_.UpdateBitrateConfigMask(bitrate_mask);
  if (config.has_value()) {
    send_side_cc_.SetBweBitrates(config->min_bitrate_bps,
                                 config->start_bitrate_bps,
                                 config->max_bitrate_bps);
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpSendTransportController.SetBitrateConfigMask: "
        << "nothing to update";
  }
}
}  // namespace webrtc
