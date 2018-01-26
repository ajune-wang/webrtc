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
#include "modules/congestion_controller/encoding_rate_controller.h"
#include "modules/congestion_controller/include/goog_cc_factory.h"
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

namespace webrtc {
namespace {

NetworkControllerFactoryInterface::uptr ControllerFactory(
    RtcEventLog* event_log) {
  return rtc::MakeUnique<GoogCcNetworkControllerFactory>(event_log);
}

void SortPacketFeedbackVector(std::vector<webrtc::PacketFeedback>* input) {
  std::sort(input->begin(), input->end(), PacketFeedbackComparator());
}

}  // namespace

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    Observer* observer,
    RtcEventLog* event_log,
    PacedSender* pacer)
    : SendSideCongestionController(clock,
                                   event_log,
                                   pacer,
                                   ControllerFactory(event_log)) {
  if (observer != nullptr)
    encoding_rate_controller_->RegisterNetworkObserver(observer);
}

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    RtcEventLog* event_log,
    PacedSender* pacer,
    NetworkControllerFactoryInterface::uptr factory)
    : clock_(clock),
      task_queue_(MakeUnique<rtc::TaskQueue>("SendSideCCQueue")),
      pacer_(pacer),
      transport_feedback_adapter_(clock_),
      send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      transport_overhead_bytes_per_packet_(0),
      encoding_rate_controller_(MakeUnique<EncodingRateController>(clock_)),
      pacer_controller_(MakeUnique<PacerController>(pacer_)) {
  controller_ = factory->Create(this);
}

SendSideCongestionController::~SendSideCongestionController() {
  // Must be destructed before any other dependant objects
  task_queue_.reset();

  controller_.reset();
  pacer_controller_.reset();
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
  TargetRateConstraints msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.min_data_rate =
      min_bitrate_bps >= 0 ? DataRate::bps(min_bitrate_bps) : DataRate::Zero();
  msg.starting_rate = start_bitrate_bps > 0 ? DataRate::bps(start_bitrate_bps)
                                            : DataRate::Infinity();
  msg.max_data_rate = max_bitrate_bps > 0 ? DataRate::bps(max_bitrate_bps)
                                          : DataRate::Infinity();

  WaitOnTask([this, msg]() { controller_->OnTargetRateConstraints(msg); });
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

  TargetRateConstraints constraints;
  constraints.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  constraints.starting_rate = DataRate::bps(start_bitrate_bps);
  constraints.min_data_rate = DataRate::bps(min_bitrate_bps);
  constraints.max_data_rate = DataRate::bps(max_bitrate_bps);
  NetworkRouteChange msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.constraints = constraints;
  WaitOnTask([this, msg]() {
    controller_->OnNetworkRouteChange(msg);
    pacer_controller_->OnNetworkRouteChange(msg);
  });
}

bool SendSideCongestionController::AvailableBandwidth(
    uint32_t* bandwidth) const {
  // TODO(srte): Remove this interface and push information about bandwidth
  // estimation to users of this class, thereby reducing synchronous calls.

  // Using locks rather than task queue here to minimize the time overhead when
  // calling this.
  rtc::CritScope cs(&network_state_lock_);
  if (last_transfer_rate_.has_value()) {
    *bandwidth = last_transfer_rate_->basis_estimate.bandwidth.bps();
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
  WaitOnTask([this, enable]() {
    streams_config_.requests_alr_probing = enable;
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::UpdateStreamsConfig() {
  RTC_DCHECK(task_queue_->IsCurrent());
  streams_config_.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  controller_->OnStreamsConfig(streams_config_);
}

int64_t SendSideCongestionController::GetPacerQueuingDelayMs() const {
  // Using locks rather than task queue here to minimize the time overhead when
  // calling this.
  rtc::CritScope cs(&network_state_lock_);
  return network_available_ ? pacer_->QueueInMs() : 0;
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
  NetworkAvailability msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.network_available = state == kNetworkUp;
  {
    rtc::CritScope cs(&network_state_lock_);
    network_available_ = msg.network_available;
  }
  task_queue_->PostTask([this, msg]() {
    controller_->OnNetworkAvailability(msg);
    pacer_controller_->OnNetworkAvailability(msg);
    encoding_rate_controller_->OnNetworkAvailability(msg);
  });
}

void SendSideCongestionController::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  transport_overhead_bytes_per_packet_ = transport_overhead_bytes_per_packet;
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
    SentPacket msg;
    msg.size = DataSize::bytes(packet->payload_size);
    msg.send_time = Timestamp::ms(packet->send_time_ms);
    task_queue_->PostTask([this, msg]() { controller_->OnSentPacket(msg); });
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
    ProcessInterval msg;
    msg.at_time = Timestamp::ms(now_ms);
    task_queue_->PostTask(
        [this, msg]() { controller_->OnProcessInterval(msg); });
  }
  if (pacer_controller_->GetPacerConfigured()) {
    PacerQueueUpdate msg;
    msg.expected_queue_time = TimeDelta::ms(pacer_->ExpectedQueueTimeMs());
    task_queue_->PostTask(
        [this, msg]() { encoding_rate_controller_->OnPacerQueueUpdate(msg); });
  }
}

void SendSideCongestionController::AddPacket(
    uint32_t ssrc,
    uint16_t sequence_number,
    size_t length,
    const PacedPacketInfo& pacing_info) {
  if (send_side_bwe_with_overhead_) {
    length += transport_overhead_bytes_per_packet_;
  }
  transport_feedback_adapter_.AddPacket(ssrc, sequence_number, length,
                                        pacing_info);
}

void SendSideCongestionController::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  int64_t feedback_time_ms = clock_->TimeInMilliseconds();

  DataSize prior_in_flight =
      DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
  transport_feedback_adapter_.OnTransportFeedback(feedback);
  MaybeUpdateOutstandingData();

  std::vector<PacketFeedback> feedback_vector =
      transport_feedback_adapter_.GetTransportFeedbackVector();
  SortPacketFeedbackVector(&feedback_vector);

  if (feedback_vector.size() > 0) {
    TransportPacketsFeedback msg;
    msg.packet_feedbacks = PacketResultsFromRtpFeedbackVector(feedback_vector);
    msg.feedback_time = Timestamp::ms(feedback_time_ms);
    msg.prior_in_flight = prior_in_flight;
    msg.data_in_flight =
        DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
    task_queue_->PostTask(
        [this, msg]() { controller_->OnTransportPacketsFeedback(msg); });
  }
}

void SendSideCongestionController::MaybeUpdateOutstandingData() {
  OutstandingData msg;
  msg.in_flight_data =
      DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
  task_queue_->PostTask(
      [this, msg]() { pacer_controller_->OnOutstandingData(msg); });
}

std::vector<PacketFeedback>
SendSideCongestionController::GetTransportFeedbackVector() const {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  return transport_feedback_adapter_.GetTransportFeedbackVector();
}

void SendSideCongestionController::WaitOnTasks() {
  rtc::Event event(false, false);
  task_queue_->PostTask([&event]() { event.Set(); });
  event.Wait(rtc::Event::kForever);
}

void SendSideCongestionController::WaitOnTask(std::function<void()> closure) {
  rtc::Event done(false, false);
  task_queue_->PostTask(rtc::NewClosure(closure, [&done] { done.Set(); }));
  done.Wait(rtc::Event::kForever);
}

void SendSideCongestionController::SetSendBitrateLimits(
    int64_t min_send_bitrate_bps,
    int64_t max_padding_bitrate_bps) {
  WaitOnTask([=]() {
    streams_config_.min_pacing_rate = DataRate::bps(min_send_bitrate_bps);
    streams_config_.max_padding_rate = DataRate::bps(max_padding_bitrate_bps);
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::SetPacingFactor(float pacing_factor) {
  WaitOnTask([=]() {
    streams_config_.pacing_factor = pacing_factor;
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::OnCongestionWindow(CongestionWindow window) {
  pacer_controller_->OnCongestionWindow(window);
}

void SendSideCongestionController::OnPacerConfig(PacerConfig config) {
  pacer_controller_->OnPacerConfig(config);
}

void SendSideCongestionController::OnProbeClusterConfig(
    ProbeClusterConfig config) {
  pacer_controller_->OnProbeClusterConfig(config);
}

void SendSideCongestionController::OnTargetTransferRate(
    TargetTransferRate transfer_rate) {
  encoding_rate_controller_->OnTargetTransferRate(transfer_rate);
  rtc::CritScope cs(&network_state_lock_);
  last_transfer_rate_ = transfer_rate;
}

void SendSideCongestionController::OnReceivedEstimatedBitrate(
    uint32_t bitrate) {
  RemoteBitrateReport msg;
  msg.receive_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.bandwidth = DataRate::bps(bitrate);
  task_queue_->PostTask(
      [this, msg]() { controller_->OnRemoteBitrateReport(msg); });
}

void SendSideCongestionController::OnReceivedRtcpReceiverReport(
    const webrtc::ReportBlockList& report_blocks,
    int64_t rtt_ms,
    int64_t now_ms) {
  OnReceivedRtcpReceiverReportBlocks(report_blocks, now_ms);

  RoundTripTimeReport report;
  report.receive_time = Timestamp::ms(now_ms);
  report.round_trip_time = TimeDelta::ms(rtt_ms);
  task_queue_->PostTask(
      [this, report]() { controller_->OnRoundTripTimeReport(report); });
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
  TransportLossReport msg;
  msg.packets_lost_delta = total_packets_lost_delta;
  msg.packets_received_delta = packets_received_delta;
  msg.receive_time = now;
  msg.start_time = last_report_block_time_;
  msg.end_time = now;
  task_queue_->PostTask(
      [this, msg]() { controller_->OnTransportLossReport(msg); });
  last_report_block_time_ = now;
}
}  // namespace webrtc
