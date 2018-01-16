/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/include/send_side_congestion_controller.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>
#include "modules/congestion_controller/bbr/include/bbr_factory.h"
#include "modules/congestion_controller/encoding_rate_controller.h"
#include "modules/congestion_controller/gocc/include/gocc_factory.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "network_control/include/network_rtp.h"
#include "network_control/include/network_types.h"
#include "network_control/include/network_units.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/socket.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

using rtc::MakeUnique;

using webrtc::network::units::Timestamp;
using webrtc::network::units::TimeDelta;
using webrtc::network::units::DataSize;
using webrtc::network::units::DataRate;

using webrtc::network::signal::QueueTaskRunner;

using webrtc::network::NetworkControllerFactoryInterface;
using webrtc::network::BbrNetworkControllerFactory;
using webrtc::network::GoccNetworkControllerFactory;

using webrtc::network::PacerController;
using webrtc::network::EncodingRateController;

using webrtc::network::CongestionWindow;
using webrtc::network::NetworkAvailability;
using webrtc::network::NetworkEstimate;
using webrtc::network::NetworkRouteChange;
using webrtc::network::OutstandingData;
using webrtc::network::PacerConfig;
using webrtc::network::PacerQueueUpdate;
using webrtc::network::ProbeClusterConfig;
using webrtc::network::ProcessInterval;
using webrtc::network::RemoteBitrateReport;
using webrtc::network::RoundTripTimeReport;
using webrtc::network::SentPacket;
using webrtc::network::StreamsConfig;
using webrtc::network::TargetRateConstraints;
using webrtc::network::TargetTransferRate;
using webrtc::network::TransportLossReport;
using webrtc::network::TransportPacketsFeedback;

namespace webrtc {
namespace {

template <typename MSG_T>
std::unique_ptr<typename MSG_T::Junction> CreateJunction(
    QueueTaskRunner* queue) {
  return rtc::MakeUnique<typename MSG_T::TaskQueueJunction>(queue);
}

NetworkControllerFactoryInterface::sptr ControllerFactoryByExperiment(
    const Clock* clock,
    RtcEventLog* event_log) {
  const char kNetworkControlExperiment[] = "WebRTC-NetworkController";

  std::string experiment_string =
      webrtc::field_trial::FindFullName(kNetworkControlExperiment);
  if (experiment_string.find("BBR") == 0)
    return std::make_shared<BbrNetworkControllerFactory>();
  else
    return std::make_shared<GoccNetworkControllerFactory>(clock, event_log);
}

void SortPacketFeedbackVector(
    std::vector<webrtc::PacketFeedback>* const input) {
  RTC_DCHECK(input);
  std::sort(input->begin(), input->end(), PacketFeedbackComparator());
}

}  // namespace

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    Observer* observer,
    RtcEventLog* event_log,
    PacedSender* pacer)
    : SendSideCongestionController(
          clock,
          event_log,
          pacer,
          ControllerFactoryByExperiment(clock, event_log)) {
  if (observer != nullptr)
    encoding_rate_controller_->RegisterNetworkObserver(observer);
}

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    RtcEventLog* event_log,
    PacedSender* pacer,
    NetworkControllerFactoryInterface::sptr factory)
    : clock_(clock),
      task_queue_(MakeUnique<rtc::TaskQueue>("SendSideCCQueue")),
      safe_queue_(MakeUnique<QueueTaskRunner>(task_queue_.get())),
      pacer_(pacer),
      transport_feedback_adapter_(clock_),
      encoding_rate_controller_(MakeUnique<EncodingRateController>(clock_)),
      pacer_controller_(MakeUnique<PacerController>(clock_, pacer_)) {
  controller_ = factory->Create();

  network::signal::QueueTaskRunner* queue = safe_queue_.get();

  NetworkRouteChangeJunction = CreateJunction<NetworkRouteChange>(queue);
  ProcessIntervalJunction = CreateJunction<ProcessInterval>(queue);
  RemoteBitrateReportJunction = CreateJunction<RemoteBitrateReport>(queue);
  RoundTripTimeReportJunction = CreateJunction<RoundTripTimeReport>(queue);
  SentPacketJunction = CreateJunction<SentPacket>(queue);
  StreamsConfigJunction = CreateJunction<StreamsConfig>(queue);
  TargetRateConstraintsJunction = CreateJunction<TargetRateConstraints>(queue);
  TransportLossReportJunction = CreateJunction<TransportLossReport>(queue);
  TransportPacketsFeedbackJunction =
      CreateJunction<TransportPacketsFeedback>(queue);

  NetworkAvailabilityJunction = CreateJunction<NetworkAvailability>(queue);
  OutstandingDataJunction = CreateJunction<OutstandingData>(queue);
  PacerQueueUpdateJunction = CreateJunction<PacerQueueUpdate>(queue);

  NetworkAvailabilityJunction->Connect(&NetworkAvailabilityCache);

  // Connect encoding rate controller
  NetworkAvailabilityJunction->Connect(
      &encoding_rate_controller_->NetworkAvailabilityReceiver);
  PacerQueueUpdateJunction->Connect(
      &encoding_rate_controller_->PacerQueueUpdateReceiver);

  // Connect pacer
  NetworkAvailabilityJunction->Connect(
      &pacer_controller_->NetworkAvailabilityReceiver);
  NetworkRouteChangeJunction->Connect(
      &pacer_controller_->NetworkRouteChangeReceiver);
  OutstandingDataJunction->Connect(&pacer_controller_->OutstandingDataReceiver);

  // Connect to controller sources
  network::NetworkControlProducers prods = controller_->GetProducers();
  // Connect controller to caches (used by stats code)
  CongestionWindowCache.ReceiveFrom(prods);
  TargetTransferRateCache.ReceiveFrom(prods);

  // Connect target rate controller to encoding rate controller
  encoding_rate_controller_->TargetTransferRateReceiver.ReceiveFrom(prods);

  // Connect network controller to pacer controller
  pacer_controller_->CongestionWindowReceiver.ReceiveFrom(prods);
  pacer_controller_->PacerConfigReceiver.ReceiveFrom(prods);
  pacer_controller_->ProbeClusterConfigReceiver.ReceiveFrom(prods);

  // Connect controller receivers to junctions
  network::NetworkInformationReceivers recvs = controller_->GetReceivers();
  NetworkAvailabilityJunction->Connect(recvs);
  NetworkRouteChangeJunction->Connect(recvs);
  ProcessIntervalJunction->Connect(recvs);
  RemoteBitrateReportJunction->Connect(recvs);
  RoundTripTimeReportJunction->Connect(recvs);
  SentPacketJunction->Connect(recvs);
  StreamsConfigJunction->Connect(recvs);
  TargetRateConstraintsJunction->Connect(recvs);
  TransportLossReportJunction->Connect(recvs);
  TransportPacketsFeedbackJunction->Connect(recvs);
}

SendSideCongestionController::~SendSideCongestionController() {
  if (safe_queue_)
    safe_queue_->StopTasks();

  NetworkAvailabilityJunction->Disconnect(&NetworkAvailabilityCache);

  network::NetworkInformationReceivers recvs = controller_->GetReceivers();
  NetworkAvailabilityJunction->Disconnect(recvs);
  NetworkRouteChangeJunction->Disconnect(recvs);
  ProcessIntervalJunction->Disconnect(recvs);
  RemoteBitrateReportJunction->Disconnect(recvs);
  RoundTripTimeReportJunction->Disconnect(recvs);
  SentPacketJunction->Disconnect(recvs);
  StreamsConfigJunction->Disconnect(recvs);
  TargetRateConstraintsJunction->Disconnect(recvs);
  TransportLossReportJunction->Disconnect(recvs);
  TransportPacketsFeedbackJunction->Disconnect(recvs);

  network::NetworkControlProducers prods = controller_->GetProducers();
  CongestionWindowCache.EndReceiveFrom(prods);
  TargetTransferRateCache.EndReceiveFrom(prods);
  encoding_rate_controller_->TargetTransferRateReceiver.EndReceiveFrom(prods);
  pacer_controller_->CongestionWindowReceiver.EndReceiveFrom(prods);
  pacer_controller_->PacerConfigReceiver.EndReceiveFrom(prods);
  pacer_controller_->ProbeClusterConfigReceiver.EndReceiveFrom(prods);

  controller_.reset();

  NetworkAvailabilityJunction->Disconnect(
      &pacer_controller_->NetworkAvailabilityReceiver);
  NetworkRouteChangeJunction->Disconnect(
      &pacer_controller_->NetworkRouteChangeReceiver);
  OutstandingDataJunction->Disconnect(
      &pacer_controller_->OutstandingDataReceiver);
  pacer_controller_.reset();

  NetworkAvailabilityJunction->Disconnect(
      &encoding_rate_controller_->NetworkAvailabilityReceiver);
  PacerQueueUpdateJunction->Disconnect(
      &encoding_rate_controller_->PacerQueueUpdateReceiver);
  encoding_rate_controller_.reset();
}

void SendSideCongestionController::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.RegisterPacketFeedbackObserver(observer);
}

void SendSideCongestionController::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.DeRegisterPacketFeedbackObserver(observer);
}

void SendSideCongestionController::RegisterNetworkObserver(Observer* observer) {
  encoding_rate_controller_->RegisterNetworkObserver(observer);
}

void SendSideCongestionController::DeRegisterNetworkObserver(
    Observer* observer) {
  encoding_rate_controller_->DeRegisterNetworkObserver(observer);
}

void SendSideCongestionController::SetBweBitrates(int min_bitrate_bps,
                                                  int start_bitrate_bps,
                                                  int max_bitrate_bps) {
  network::TargetRateConstraints msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.min_data_rate =
      min_bitrate_bps >= 0 ? DataRate::bps(min_bitrate_bps) : DataRate::Zero();
  msg.starting_rate = start_bitrate_bps > 0 ? DataRate::bps(start_bitrate_bps)
                                            : DataRate::Infinity();
  msg.max_data_rate = max_bitrate_bps > 0 ? DataRate::bps(max_bitrate_bps)
                                          : DataRate::Infinity();
  TargetRateConstraintsJunction->BeginInvoke(msg);
}

// TODO(holmer): Split this up and use SetBweBitrates in combination with
// OnNetworkRouteChanged.
void SendSideCongestionController::OnNetworkRouteChanged(
    const rtc::NetworkRoute& network_route,
    int start_bitrate_bps,
    int min_bitrate_bps,
    int max_bitrate_bps) {
  transport_feedback_adapter_.SetNetworkIds(network_route.local_network_id,
                                            network_route.remote_network_id);

  network::TargetRateConstraints constraints;
  constraints.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  constraints.starting_rate = DataRate::bps(start_bitrate_bps);
  constraints.min_data_rate = DataRate::bps(min_bitrate_bps);
  constraints.max_data_rate = DataRate::bps(max_bitrate_bps);
  network::NetworkRouteChange msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.constraints = constraints;
  NetworkRouteChangeJunction->BeginInvoke(msg);
}

bool SendSideCongestionController::AvailableBandwidth(
    uint32_t* bandwidth) const {
  auto last_rate = TargetTransferRateCache.GetLastMessage();
  if (last_rate.has_value()) {
    *bandwidth = last_rate->basis_estimate.bandwidth.bps();
    return true;
  }
  return false;
}

RtcpBandwidthObserver* SendSideCongestionController::GetBandwidthObserver() {
  return this;
}

RateLimiter* SendSideCongestionController::GetRetransmissionRateLimiter() {
  return encoding_rate_controller_->GetRetransmissionRateLimiter();
}

void SendSideCongestionController::EnablePeriodicAlrProbing(bool enable) {
  rtc::CritScope cs(&streams_config_lock_);
  streams_config_.requests_alr_probing = true;
  StreamsConfigJunction->BeginInvoke(streams_config_);
}

int64_t SendSideCongestionController::GetPacerQueuingDelayMs() const {
  auto availability = NetworkAvailabilityCache.GetLastMessage();
  bool network_available =
      availability ? availability->network_available : true;
  return network_available ? pacer_->QueueInMs() : 0;
}

int64_t SendSideCongestionController::GetFirstPacketTimeMs() const {
  return pacer_->FirstSentPacketTimeMs();
}

TransportFeedbackObserver*
SendSideCongestionController::GetTransportFeedbackObserver() {
  return this;
}

void SendSideCongestionController::SignalNetworkState(NetworkState state) {
  RTC_LOG(LS_INFO) << "SignalNetworkState "
                   << (state == kNetworkUp ? "Up" : "Down");
  network::NetworkAvailability msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.network_available = state == kNetworkUp;
  NetworkAvailabilityJunction->BeginInvoke(msg);
}

void SendSideCongestionController::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  transport_feedback_adapter_.SetTransportOverhead(
      rtc::dchecked_cast<int>(transport_overhead_bytes_per_packet));
}

void SendSideCongestionController::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  // We're not interested in packets without an id, which may be stun packets,
  // etc, sent on the same transport.
  if (sent_packet.packet_id == -1)
    return;
  transport_feedback_adapter_.OnSentPacket(sent_packet.packet_id,
                                           sent_packet.send_time_ms);
  MaybeUpdateOutstandingData();
  auto packet = transport_feedback_adapter_.GetPacket(sent_packet.packet_id);
  if (packet.has_value()) {
    network::SentPacket msg;
    msg.size = DataSize::bytes(packet->payload_size);
    msg.send_time = Timestamp::ms(packet->send_time_ms);
    SentPacketJunction->OnMessage(msg);
  }
}

void SendSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                               int64_t max_rtt_ms) {
  /* Ignoring this */
}

int64_t SendSideCongestionController::TimeUntilNextProcess() {
  TimeDelta process_interval = controller_->GetProcessInterval();
  const int kMaxProcessInterval = 60 * 1000;
  if (process_interval.IsInfinite())
    return kMaxProcessInterval;
  int64_t next_process_ms = last_process_update_ms_ + process_interval.ms();
  int64_t time_until_next_process =
      next_process_ms - clock_->TimeInMilliseconds();
  return std::max<int64_t>(time_until_next_process, 0);
}

void SendSideCongestionController::Process() {
  int64_t now_ms = clock_->TimeInMilliseconds();
  last_process_update_ms_ = now_ms;
  {
    network::ProcessInterval msg;
    msg.at_time = Timestamp::ms(now_ms);
    ProcessIntervalJunction->OnMessage(msg);
  }
  if (pacer_controller_->GetPacerConfigured()) {
    network::PacerQueueUpdate msg;
    msg.expected_queue_time = TimeDelta::ms(pacer_->ExpectedQueueTimeMs());
    PacerQueueUpdateJunction->OnMessage(msg);
  }
}

void SendSideCongestionController::AddPacket(
    uint32_t ssrc,
    uint16_t sequence_number,
    size_t length,
    const PacedPacketInfo& pacing_info) {
  transport_feedback_adapter_.AddPacket(ssrc, sequence_number, length,
                                        pacing_info);
}

void SendSideCongestionController::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  int64_t feedback_time_ms = clock_->TimeInMilliseconds();

  auto prior_in_flight =
      DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
  transport_feedback_adapter_.OnTransportFeedback(feedback);
  MaybeUpdateOutstandingData();

  std::vector<PacketFeedback> feedback_vector =
      transport_feedback_adapter_.GetTransportFeedbackVector();
  SortPacketFeedbackVector(&feedback_vector);

  if (feedback_vector.size() > 0) {
    network::TransportPacketsFeedback msg =
        network::TransportPacketsFeedbackFromRtpFeedbackVector(
            feedback_vector, feedback_time_ms);
    msg.prior_in_flight = prior_in_flight;
    msg.data_in_flight =
        DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
    TransportPacketsFeedbackJunction->OnMessage(msg);
  }
}

void SendSideCongestionController::MaybeUpdateOutstandingData() {
  // Only process if a congestion window has been set to save time otherwise
  if (CongestionWindowCache.GetLastMessage()) {
    network::OutstandingData msg;
    msg.in_flight_data =
        DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
    OutstandingDataJunction->OnMessage(msg);
  }
}

std::vector<PacketFeedback>
SendSideCongestionController::GetTransportFeedbackVector() const {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  return transport_feedback_adapter_.GetTransportFeedbackVector();
}

void SendSideCongestionController::WaitOnControllers() {
  WaitOnOneQueuedTask();  // Wait for network controller
  WaitOnOneQueuedTask();  // Wait for pacer controller
  WaitOnOneQueuedTask();  // Wait for encoding rate controller
}

void SendSideCongestionController::WaitOnOneQueuedTask() {
  rtc::Event event(false, false);
  task_queue_->PostTask([&event]() { event.Set(); });
  event.Wait(rtc::Event::kForever);
}

void SendSideCongestionController::SetSendBitrateLimits(
    int64_t min_send_bitrate_bps,
    int64_t max_padding_bitrate_bps) {
  rtc::CritScope cs(&streams_config_lock_);
  streams_config_.min_pacing_rate = DataRate::bps(min_send_bitrate_bps);
  streams_config_.max_padding_rate = DataRate::bps(max_padding_bitrate_bps);
  StreamsConfigJunction->BeginInvoke(streams_config_);
}

void SendSideCongestionController::SetPacingFactor(float pacing_factor) {
  rtc::CritScope cs(&streams_config_lock_);
  streams_config_.pacing_factor = pacing_factor;
  StreamsConfigJunction->BeginInvoke(streams_config_);
}

void SendSideCongestionController::OnReceivedEstimatedBitrate(
    uint32_t bitrate) {
  network::RemoteBitrateReport msg;
  msg.receive_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.bandwidth = DataRate::bps(bitrate);
  RemoteBitrateReportJunction->OnMessage(msg);
}

void SendSideCongestionController::OnReceivedRtcpReceiverReport(
    const webrtc::ReportBlockList& report_blocks,
    int64_t rtt_ms,
    int64_t now_ms) {
  OnReceivedRtcpReceiverReportBlocks(report_blocks, now_ms);

  network::RoundTripTimeReport report;
  report.receive_time = Timestamp::ms(now_ms);
  report.round_trip_time = TimeDelta::ms(rtt_ms);
  RoundTripTimeReportJunction->OnMessage(report);
}

void SendSideCongestionController::OnReceivedRtcpReceiverReportBlocks(
    const ReportBlockList& report_blocks,
    int64_t now_ms) {
  if (report_blocks.empty())
    return;

  int total_packets_lost_delta = 0;
  int total_packets_delta = 0;

  // Compute the packet loss from all report blocks.
  for (const RTCPReportBlock& report_block : report_blocks) {
    auto it = last_report_blocks_.find(report_block.source_ssrc);
    if (it != last_report_blocks_.end()) {
      auto number_of_packets = report_block.extended_highest_sequence_number -
                               it->second.extended_highest_sequence_number;
      total_packets_delta += number_of_packets;
      auto lost_delta = report_block.packets_lost - it->second.packets_lost;
      total_packets_lost_delta += lost_delta;
    }
    last_report_blocks_[report_block.source_ssrc] = report_block;
  }
  // Can only compute delta if there has been previous blocks to compare to. If
  // not, total_packets_delta will be unchanged and there's nothing more to do.
  if (!total_packets_delta)
    return;
  int packets_received_delta = total_packets_delta - total_packets_lost_delta;
  // To detect lost packets, at least one packet has to be received. This check
  // is needed to avoid bandwith detection update in
  // VideoSendStreamTest.SuspendBelowMinBitrate

  if (packets_received_delta < 1)
    return;
  Timestamp now = Timestamp::ms(now_ms);
  network::TransportLossReport msg;
  msg.packets_lost_delta = total_packets_lost_delta;
  msg.packets_received_delta = packets_received_delta;
  msg.receive_time = now;
  msg.start_time = last_report_block_time_;
  msg.end_time = now;

  TransportLossReportJunction->OnMessage(msg);
  last_report_block_time_ = now;
}
}  // namespace webrtc
