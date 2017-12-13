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

#include "modules/congestion_controller/network_controllers.h"
#include "modules/congestion_controller/probe_controller.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "network_control/include/network_rtp.h"
#include "network_control/include/network_types.h"
#include "network_control/include/network_units.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/socket.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

using std::placeholders::_1;
using webrtc::network::units::Timestamp;
using webrtc::network::units::TimeDelta;
using webrtc::network::units::DataSize;
using webrtc::network::units::DataRate;

using webrtc::network::CongestionWindow;
using webrtc::network::Invalidation;
using webrtc::network::NetworkAvailability;
using webrtc::network::NetworkEstimate;
using webrtc::network::PacerConfig;
using webrtc::network::PacerState;
using webrtc::network::ProbeClusterConfig;
using webrtc::network::ProcessInterval;
using webrtc::network::TargetTransferRate;

namespace webrtc {
namespace {
const char kPacerPushbackExperiment[] = "WebRTC-PacerPushbackExperiment";
// TODO(srte): Use shared constant or fix tests
const float kDefaultPaceMultiplier = 2.5f;
static const int64_t kRetransmitWindowSizeMs = 500;

void SortPacketFeedbackVector(
    std::vector<webrtc::PacketFeedback>* const input) {
  RTC_DCHECK(input);
  std::sort(input->begin(), input->end(), PacketFeedbackComparator());
}

template <typename T>
std::unique_ptr<network::signal::HandlingReceiver<T>> CreateHandler(
    rtc::TaskQueue* optional_queue,
    rtc::CriticalSection* optional_lock = nullptr) {
  if (optional_queue != nullptr) {
    using network::signal::TaskQueueReceiver;
    return std::move(rtc::MakeUnique<TaskQueueReceiver<T>>(optional_queue));
  } else {
    RTC_CHECK(optional_lock);
    using network::signal::LockedReceiver;
    return std::move(rtc::MakeUnique<LockedReceiver<T>>(optional_lock));
  }
  RTC_NOTREACHED();
}
}  // namespace

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    Observer* observer,
    RtcEventLog* event_log,
    PacedSender* pacer)
    : clock_(clock),
      observer_(observer),
      event_log_(event_log),
      pacer_(pacer),
      retransmission_rate_limiter_(
          new RateLimiter(clock, kRetransmitWindowSizeMs)),
      transport_feedback_adapter_(clock_),
      last_reported_target_bitrate_bps_(0),
      last_reported_fraction_loss_(0),
      last_reported_rtt_ms_(0),
      pacer_pushback_experiment_(
          webrtc::field_trial::IsEnabled(kPacerPushbackExperiment)),
      using_task_queue_(true) {
  rtc::TaskQueue* queue = nullptr;
  if (using_task_queue_) {
    task_queue_ =
        rtc::MakeUnique<rtc::TaskQueue>("SendSideCongestionControllerQueue");
    queue = task_queue_.get();
    controller_ = network::CreateDelayBasedNetworkController(clock_, event_log_,
                                                             task_queue_.get());
  } else {
    controller_ = network::CreateDelayBasedNetworkController(clock, event_log);
  }
  rtc::CriticalSection* lock = &controller_lock_;

  // Internal connections
  CongestionInvalidationReceiver = CreateHandler<Invalidation>(queue, lock);
  NetworkAvailabilityReceiver = CreateHandler<NetworkAvailability>(queue, lock);
  NetworkInvalidationReceiver = CreateHandler<Invalidation>(queue, lock);

  CongestionInvalidationJunction.Connect(CongestionInvalidationReceiver.get());
  NetworkAvailabilityJunction.Connect(NetworkAvailabilityReceiver.get());
  NetworkInvalidationJunction.Connect(NetworkInvalidationReceiver.get());

  // Pacer connections
  PacerConfigReceiver = CreateHandler<PacerConfig>(queue, lock);
  PacerStateReceiver = CreateHandler<PacerState>(queue, lock);
  ProbeClusterConfigReceiver = CreateHandler<ProbeClusterConfig>(queue, lock);

  using This = SendSideCongestionController;
  using std::bind;

  PacerConfigReceiver->SetHandler(bind(&This::OnPacerConfig, this, _1));
  PacerStateReceiver->SetHandler(bind(&This::OnPacerState, this, _1));
  ProbeClusterConfigReceiver->SetHandler(
      bind(&This::OnProbeClusterConfig, this, _1));

  CongestionInvalidationReceiver->SetHandler(
      bind(&This::OnCongestionInvalidation, this, _1));
  NetworkAvailabilityReceiver->SetHandler(
      bind(&This::OnNetworkAvailability, this, _1));
  NetworkInvalidationReceiver->SetHandler(
      bind(&This::OnNetworkInvalidation, this, _1));
  // Create handlers for controller messages
  CongestionWindowReceiver = CreateHandler<CongestionWindow>(queue, lock);
  TargetTransferRateReceiver = CreateHandler<TargetTransferRate>(queue, lock);

  // Set handlers for controller messages
  CongestionWindowReceiver->SetHandler(
      bind(&This::OnCongestionWindow, this, _1));
  TargetTransferRateReceiver->SetHandler(
      bind(&This::OnTargetTransferRate, this, _1));

  PacerStateJunction.Connect(PacerStateReceiver.get());

  // Connect to controller sources
  network::NetworkControlProducers::uptr prods = controller_->GetProducers();
  prods->CongestionWindowProducer->Connect(CongestionWindowReceiver.get());
  prods->TargetTransferRateProducer->Connect(TargetTransferRateReceiver.get());

  // Connect controller to internal pacer receivers
  prods->PacerConfigProducer->Connect(PacerConfigReceiver.get());
  prods->ProbeClusterConfigProducer->Connect(ProbeClusterConfigReceiver.get());

  // Connect controller receivers to junctions
  network::NetworkControlReceivers::uptr recvs = controller_->GetReceivers();
  SentPacketJunction.Connect(recvs->SentPacketReceiver);
  TransportPacketsFeedbackJunction.Connect(
      recvs->TransportPacketsFeedbackReceiver);
  TransportLossReportJunction.Connect(recvs->TransportLossReportReceiver);
  RoundTripTimeReportJunction.Connect(recvs->RoundTripTimeReportReceiver);
  RemoteBitrateReportJunction.Connect(recvs->RemoteBitrateReportReceiver);
  TransferRateConstraintsJunction.Connect(
      recvs->TransferRateConstraintsReceiver);
  StreamsConfigJunction.Connect(recvs->StreamsConfigReceiver);
  NetworkAvailabilityJunction.Connect(recvs->NetworkAvailabilityReceiver);
  NetworkRouteChangeJunction.Connect(recvs->NetworkRouteChangeReceiver);
  ProcessIntervalJunction.Connect(recvs->ProcessIntervalReceiver);
  SetPacingFactor(kDefaultPaceMultiplier);
}

SendSideCongestionController::~SendSideCongestionController() {
  NetworkAvailabilityJunction.Disconnect(NetworkAvailabilityReceiver.get());

  network::NetworkControlReceivers::uptr recvs = controller_->GetReceivers();
  NetworkAvailabilityJunction.Disconnect(recvs->NetworkAvailabilityReceiver);
  SentPacketJunction.Disconnect(recvs->SentPacketReceiver);
  TransportPacketsFeedbackJunction.Disconnect(
      recvs->TransportPacketsFeedbackReceiver);
  TransportLossReportJunction.Disconnect(recvs->TransportLossReportReceiver);
  RoundTripTimeReportJunction.Disconnect(recvs->RoundTripTimeReportReceiver);
  RemoteBitrateReportJunction.Disconnect(recvs->RemoteBitrateReportReceiver);
  TransferRateConstraintsJunction.Disconnect(
      recvs->TransferRateConstraintsReceiver);
  StreamsConfigJunction.Disconnect(recvs->StreamsConfigReceiver);
  NetworkRouteChangeJunction.Disconnect(recvs->NetworkRouteChangeReceiver);
  ProcessIntervalJunction.Disconnect(recvs->ProcessIntervalReceiver);

  network::NetworkControlProducers::uptr prods = controller_->GetProducers();
  prods->CongestionWindowProducer->Disconnect(CongestionWindowReceiver.get());
  prods->TargetTransferRateProducer->Disconnect(
      TargetTransferRateReceiver.get());
  prods->PacerConfigProducer->Disconnect(PacerConfigReceiver.get());
  prods->ProbeClusterConfigProducer->Disconnect(
      ProbeClusterConfigReceiver.get());

  CongestionInvalidationJunction.Disconnect(
      CongestionInvalidationReceiver.get());
  NetworkInvalidationJunction.Disconnect(NetworkInvalidationReceiver.get());
  PacerStateJunction.Disconnect(PacerStateReceiver.get());
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
  rtc::CritScope cs(&observer_lock_);
  RTC_DCHECK(observer_ == nullptr);
  observer_ = observer;
}

void SendSideCongestionController::DeRegisterNetworkObserver(
    Observer* observer) {
  rtc::CritScope cs(&observer_lock_);
  RTC_DCHECK_EQ(observer_, observer);
  observer_ = nullptr;
}

void SendSideCongestionController::SetBweBitrates(int min_bitrate_bps,
                                                  int start_bitrate_bps,
                                                  int max_bitrate_bps) {
  network::TargetRateConstraints msg;
  msg.starting_rate = DataRate::bps(start_bitrate_bps);
  msg.min_data_rate = DataRate::bps(min_bitrate_bps);
  msg.max_data_rate = DataRate::bps(max_bitrate_bps);
  TransferRateConstraintsJunction.OnMessage(msg);
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
  constraints.starting_rate = DataRate::bps(start_bitrate_bps);
  constraints.min_data_rate = DataRate::bps(min_bitrate_bps);
  constraints.max_data_rate = DataRate::bps(max_bitrate_bps);
  network::NetworkRouteChange msg;
  msg.constraints = constraints;
  NetworkRouteChangeJunction.OnMessage(msg);
}

bool SendSideCongestionController::AvailableBandwidth(
    uint32_t* bandwidth) const {
  rtc::CritScope cs(&controller_lock_);
  if (current_target_rate_.has_value()) {
    *bandwidth = current_target_rate_->basis_estimate.bandwidth.bps();
    return true;
  }
  return false;
}

RtcpBandwidthObserver* SendSideCongestionController::GetBandwidthObserver() {
  return this;
}

RateLimiter* SendSideCongestionController::GetRetransmissionRateLimiter() {
  return retransmission_rate_limiter_.get();
}

void SendSideCongestionController::EnablePeriodicAlrProbing(bool enable) {
  rtc::CritScope cs(&streams_config_lock_);
  streams_config_.requests_alr_probing = true;
  StreamsConfigJunction.OnMessage(streams_config_);
}

int64_t SendSideCongestionController::GetPacerQueuingDelayMs() const {
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
  network::NetworkAvailability msg;
  msg.network_available = state == kNetworkUp;
  NetworkAvailabilityJunction.OnMessage(msg);
}

void SendSideCongestionController::OnNetworkAvailability(
    NetworkAvailability msg) {
  SetPacerState(!msg.network_available);
  network_available_ = msg.network_available;
  NetworkInvalidationJunction.OnMessage(Invalidation());
}

void SendSideCongestionController::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  transport_feedback_adapter_.SetTransportOverhead(
      transport_overhead_bytes_per_packet);
}

void SendSideCongestionController::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  // We're not interested in packets without an id, which may be stun packets,
  // etc, sent on the same transport.
  if (sent_packet.packet_id == -1)
    return;
  transport_feedback_adapter_.OnSentPacket(sent_packet.packet_id,
                                           sent_packet.send_time_ms);
  CongestionInvalidationJunction.OnMessage(Invalidation());
  auto packet = transport_feedback_adapter_.GetPacket(sent_packet.packet_id);
  if (packet.has_value()) {
    network::SentPacket msg;
    msg.size = DataSize::bytes(packet->payload_size);
    msg.send_time = Timestamp::ms(packet->send_time_ms);
    SentPacketJunction.OnMessage(msg);
  }
}

void SendSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                               int64_t max_rtt_ms) {
  /* Ignoring this */
}

int64_t SendSideCongestionController::TimeUntilNextProcess() {
  TimeDelta process_interval = controller_->GetProcessInterval();
  int64_t next_process_ms = last_process_update_ms_ + process_interval.ms();
  int64_t time_until_next_process =
      next_process_ms - clock_->TimeInMilliseconds();
  return std::max<int64_t>(time_until_next_process, 0);
}

void SendSideCongestionController::Process() {
  int64_t now_ms = clock_->TimeInMilliseconds();
  last_process_update_ms_ = now_ms;
  network::ProcessInterval msg;
  msg.at_time = Timestamp::ms(now_ms);
  ProcessIntervalJunction.OnMessage(msg);
  NetworkInvalidationJunction.OnMessage(Invalidation());
}

void SendSideCongestionController::SetPacerState(bool pause_pacer) {
  PacerState new_state;
  new_state.paused = pause_pacer;
  PacerStateJunction.OnMessage(new_state);
}

void SendSideCongestionController::OnPacerState(network::PacerState msg) {
  if (msg.paused && !pacer_paused_)
    pacer_->Pause();
  else if (!msg.paused && pacer_paused_)
    pacer_->Resume();
  pacer_paused_ = msg.paused;
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
  transport_feedback_adapter_.OnTransportFeedback(feedback);
  CongestionInvalidationJunction.OnMessage(Invalidation());

  std::vector<PacketFeedback> feedback_vector =
      transport_feedback_adapter_.GetTransportFeedbackVector();
  SortPacketFeedbackVector(&feedback_vector);

  if (feedback_vector.size() > 0) {
    network::TransportPacketsFeedback msg =
        network::TransportPacketsFeedbackFromRtpFeedbackVector(
            feedback_vector, feedback_time_ms);
    TransportPacketsFeedbackJunction.OnMessage(msg);
  }
}

void SendSideCongestionController::OnCongestionWindow(
    network::CongestionWindow congestion_window) {
  congestion_window_ = congestion_window;
  CongestionInvalidationJunction.OnMessage({});
}

void SendSideCongestionController::OnPacerConfig(network::PacerConfig msg) {
  DataRate pacing_rate = msg.data_window / msg.time_window;
  DataRate padding_rate = msg.pad_window / msg.time_window;
  pacer_->SetPacingRates(pacing_rate.bps(), padding_rate.bps());
}

void SendSideCongestionController::OnProbeClusterConfig(
    network::ProbeClusterConfig config) {
  int64_t bitrate_bps = config.target_data_rate.bps();
  pacer_->CreateProbeCluster(bitrate_bps);
}

void SendSideCongestionController::OnCongestionInvalidation(
    network::Invalidation) {
  if (!congestion_window_ || !congestion_window_->enabled)
    return;
  size_t max_outstanding_bytes = congestion_window_->data_window.bytes();
  size_t num_outstanding_bytes =
      transport_feedback_adapter_.GetOutstandingBytes();

  RTC_LOG(LS_INFO) << clock_->TimeInMilliseconds()
                   << " Outstanding bytes: " << num_outstanding_bytes
                   << " pacer queue: " << pacer_->QueueInMs()
                   << " max outstanding: " << max_outstanding_bytes;
  bool pause_pacer = num_outstanding_bytes > max_outstanding_bytes;
  SetPacerState(pause_pacer);
}

std::vector<PacketFeedback>
SendSideCongestionController::GetTransportFeedbackVector() const {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  return transport_feedback_adapter_.GetTransportFeedbackVector();
}

void SendSideCongestionController::Sync() {
  if (!using_task_queue_)
    return;
  Wait();  // Wait for possible OnProcessInterval
  Wait();  // Wait for possible Inner OnProcessInterval
  Wait();  // Wait for possible OnNetworkEstimate
  Wait();  // Wait for possible OnTargetTransferRate
  Wait();  // Wait for possible OnPacerConfig
  Wait();  // Wait for possible OnClusterConfig
  Wait();  // Wait for possible OnCongestionWindow
  Wait();  // Wait for possible OnCongestionInvalidation
  Wait();  // Wait for possible OnNetworkInvalidation
}

void SendSideCongestionController::Wait() {
  rtc::Event event(false, false);
  task_queue_->PostTask([&event]() { event.Set(); });
  event.Wait(rtc::Event::kForever);
}

void SendSideCongestionController::OnTargetTransferRate(
    network::TargetTransferRate target_rate) {
  retransmission_rate_limiter_->SetMaxRate(
      target_rate.basis_estimate.bandwidth.bps());
  {
    rtc::CritScope cs(&controller_lock_);
    current_target_rate_ = target_rate;
  }
  NetworkInvalidationJunction.OnMessage(Invalidation());
}

void SendSideCongestionController::OnNetworkInvalidation(
    network::Invalidation) {
  uint32_t target_bitrate_bps;
  uint8_t fraction_loss;
  int64_t rtt_ms;

  if (!current_target_rate_.has_value())
    return;
  target_bitrate_bps = current_target_rate_->target_rate.bps();
  fraction_loss = current_target_rate_->basis_estimate.GetLossRatioUint8();
  rtt_ms = current_target_rate_->basis_estimate.round_trip_time.ms();
  int64_t probing_interval_ms =
      current_target_rate_->basis_estimate.bwe_period.ms();

  if (!network_available_) {
    target_bitrate_bps = 0;
  } else if (!pacer_pushback_experiment_) {
    target_bitrate_bps = IsSendQueueFull() ? 0 : target_bitrate_bps;
  } else {
    int64_t queue_length_ms = pacer_->ExpectedQueueTimeMs();

    if (queue_length_ms == 0) {
      encoding_rate_ = 1.0;
    } else if (queue_length_ms > 50) {
      float encoding_rate = 1.0 - queue_length_ms / 1000.0;
      encoding_rate_ = std::min(encoding_rate_, encoding_rate);
      encoding_rate_ = std::max(encoding_rate_, 0.0f);
    }

    target_bitrate_bps *= encoding_rate_;
    target_bitrate_bps = target_bitrate_bps < 50000 ? 0 : target_bitrate_bps;
  }
  if (HasNetworkParametersToReportChanged(target_bitrate_bps, fraction_loss,
                                          rtt_ms)) {
    rtc::CritScope cs(&observer_lock_);
    if (observer_) {
      observer_->OnNetworkChanged(target_bitrate_bps, fraction_loss, rtt_ms,
                                  probing_interval_ms);
    }
  }
}

bool SendSideCongestionController::HasNetworkParametersToReportChanged(
    int64_t target_bitrate_bps,
    uint8_t fraction_loss,
    int64_t rtt_ms) {
  bool changed = last_reported_target_bitrate_bps_ != target_bitrate_bps ||
                 (target_bitrate_bps > 0 &&
                  (last_reported_fraction_loss_ != fraction_loss ||
                   last_reported_rtt_ms_ != rtt_ms));
  if (changed &&
      (last_reported_target_bitrate_bps_ == 0 || target_bitrate_bps == 0)) {
    RTC_LOG(LS_INFO) << "Bitrate estimate state changed, BWE: "
                     << target_bitrate_bps << " bps.";
  }
  last_reported_target_bitrate_bps_ = target_bitrate_bps;
  last_reported_fraction_loss_ = fraction_loss;
  last_reported_rtt_ms_ = rtt_ms;
  return changed;
}

void SendSideCongestionController::SetSendBitrateLimits(
    int64_t min_send_bitrate_bps,
    int64_t max_padding_bitrate_bps) {
  rtc::CritScope cs(&streams_config_lock_);
  streams_config_.min_pacing_rate = DataRate::bps(min_send_bitrate_bps);
  streams_config_.max_padding_rate = DataRate::bps(max_padding_bitrate_bps);
  StreamsConfigJunction.OnMessage(streams_config_);
}

void SendSideCongestionController::SetPacingFactor(float pacing_factor) {
  rtc::CritScope cs(&streams_config_lock_);
  streams_config_.pacing_factor = pacing_factor;
  StreamsConfigJunction.OnMessage(streams_config_);
}

float SendSideCongestionController::GetPacingFactor() const {
  rtc::CritScope cs(&streams_config_lock_);
  return streams_config_.pacing_factor;
}

bool SendSideCongestionController::IsSendQueueFull() const {
  return pacer_->ExpectedQueueTimeMs() > PacedSender::kMaxQueueLengthMs;
}

void SendSideCongestionController::OnReceivedEstimatedBitrate(
    uint32_t bitrate) {
  network::RemoteBitrateReport msg;
  msg.receive_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.bandwidth = DataRate::bps(bitrate);
  RemoteBitrateReportJunction.OnMessage(msg);
}

void SendSideCongestionController::OnReceivedRtcpReceiverReport(
    const webrtc::ReportBlockList& report_blocks,
    int64_t rtt_ms,
    int64_t now_ms) {
  OnReceivedRtcpReceiverReportBlocks(report_blocks, now_ms);

  network::RoundTripTimeReport report;
  report.receive_time = Timestamp::ms(now_ms);
  report.round_trip_time = TimeDelta::ms(rtt_ms);
  RoundTripTimeReportJunction.OnMessage(report);
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

  TransportLossReportJunction.OnMessage(msg);
  last_report_block_time_ = now;
}
}  // namespace webrtc
