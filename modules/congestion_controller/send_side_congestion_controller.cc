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

static const int64_t kRetransmitWindowSizeMs = 500;

NetworkControllerFactoryInterface::uptr ControllerFactory(
    RtcEventLog* event_log) {
  return rtc::MakeUnique<GoogCcNetworkControllerFactory>(event_log);
}

void SortPacketFeedbackVector(std::vector<webrtc::PacketFeedback>* input) {
  std::sort(input->begin(), input->end(), PacketFeedbackComparator());
}

PacketResult NetworkPacketFeedbackFromRtpPacketFeedback(
    const webrtc::PacketFeedback& pf) {
  PacketResult feedback;
  if (pf.arrival_time_ms == webrtc::PacketFeedback::kNotReceived)
    feedback.receive_time = Timestamp::Infinity();
  else
    feedback.receive_time = Timestamp::ms(pf.arrival_time_ms);
  if (pf.send_time_ms != webrtc::PacketFeedback::kNoSendTime) {
    feedback.sent_packet = SentPacket();
    feedback.sent_packet->send_time = Timestamp::ms(pf.send_time_ms);
    feedback.sent_packet->size = DataSize::bytes(pf.payload_size);
    feedback.sent_packet->pacing_info = pf.pacing_info;
  }
  return feedback;
}
std::vector<PacketResult> PacketResultsFromRtpFeedbackVector(
    const std::vector<PacketFeedback>& feedback_vector) {
  RTC_DCHECK(std::is_sorted(feedback_vector.begin(), feedback_vector.end(),
                            PacketFeedbackComparator()));

  std::vector<PacketResult> packet_feedbacks;
  packet_feedbacks.reserve(feedback_vector.size());
  for (const PacketFeedback& rtp_feedback : feedback_vector) {
    auto feedback = NetworkPacketFeedbackFromRtpPacketFeedback(rtp_feedback);
    packet_feedbacks.push_back(feedback);
  }
  return packet_feedbacks;
}

TargetRateConstraints ConvertConstraints(int min_bitrate_bps,
                                        int max_bitrate_bps,
                                        int start_bitrate_bps,
                                        const Clock* clock) {
  TargetRateConstraints msg;
  msg.at_time = Timestamp::ms(clock->TimeInMilliseconds());
  msg.min_data_rate =
      min_bitrate_bps >= 0 ? DataRate::bps(min_bitrate_bps) : DataRate::Zero();
  msg.starting_rate = start_bitrate_bps > 0 ? DataRate::bps(start_bitrate_bps)
                                            : DataRate::kNotInitialized;
  msg.max_data_rate = max_bitrate_bps > 0 ? DataRate::bps(max_bitrate_bps)
                                          : DataRate::Infinity();
  return msg;
}
}  // namespace

class SendSideCongestionController::ControlRouter
    : public NetworkControllerObserver {
 public:
  ControlRouter(EncodingRateController* encoding_rate_controller,
                PacerController* pacer_controller,
                const Clock* clock)
      : encoding_rate_controller_(encoding_rate_controller),
        pacer_controller_(pacer_controller),
        retransmission_rate_limiter_(clock, kRetransmitWindowSizeMs) {}

  void OnCongestionWindow(CongestionWindow window) override {
    pacer_controller_->OnCongestionWindow(window);
  }

  void OnPacerConfig(PacerConfig config) override {
    pacer_controller_->OnPacerConfig(config);
    rtc::CritScope cs(&state_lock_);
    pacer_configured_ = true;
  }

  void OnProbeClusterConfig(ProbeClusterConfig config) override {
    pacer_controller_->OnProbeClusterConfig(config);
  }

  void OnTargetTransferRate(TargetTransferRate target_rate) override {
    retransmission_rate_limiter_.SetMaxRate(
        target_rate.network_estimate.bandwidth.bps());
    encoding_rate_controller_->OnTargetTransferRate(target_rate);
    rtc::CritScope cs(&state_lock_);
    last_target_rate_ = target_rate;
  }

  rtc::Optional<TargetTransferRate> last_transfer_rate() {
    rtc::CritScope cs(&state_lock_);
    return last_target_rate_;
  }

  bool pacer_configured() {
    rtc::CritScope cs(&state_lock_);
    return pacer_configured_;
  }

  RateLimiter* retransmission_rate_limiter() {
    return &retransmission_rate_limiter_;
  }

 private:
  EncodingRateController* encoding_rate_controller_;
  PacerController* pacer_controller_;
  RateLimiter retransmission_rate_limiter_;

  rtc::CriticalSection state_lock_;
  rtc::Optional<TargetTransferRate> last_target_rate_
      RTC_GUARDED_BY(state_lock_);
  bool pacer_configured_ RTC_GUARDED_BY(state_lock_) = false;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ControlRouter);
};

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
    RegisterNetworkObserver(observer);
}

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    RtcEventLog* event_log,
    PacedSender* pacer,
    NetworkControllerFactoryInterface::uptr controller_factory)
    : clock_(clock),
      pacer_(pacer),
      transport_feedback_adapter_(clock_),
      encoding_rate_controller_(MakeUnique<EncodingRateController>(clock_)),
      pacer_controller_(MakeUnique<PacerController>(pacer_)),
      router_(MakeUnique<ControlRouter>(encoding_rate_controller_.get(),
                                        pacer_controller_.get(),
                                        clock_)),
      controller_(controller_factory->Create(router_.get())),
      process_interval_(controller_factory->GetProcessInterval()),
      send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      transport_overhead_bytes_per_packet_(0),
      network_available_(true),
      task_queue_(MakeUnique<rtc::TaskQueue>("SendSideCCQueue")) {}

SendSideCongestionController::~SendSideCongestionController() {
  // Must be destructed before any objects used by calls on the task queue.
  task_queue_.reset();
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
  WaitOnTask([this, observer]() {
    encoding_rate_controller_->RegisterNetworkObserver(observer);
  });
}

void SendSideCongestionController::DeRegisterNetworkObserver(
    Observer* observer) {
  WaitOnTask([this, observer]() {
    encoding_rate_controller_->DeRegisterNetworkObserver(observer);
  });
}

void SendSideCongestionController::SetBweBitrates(int min_bitrate_bps,
                                                  int start_bitrate_bps,
                                                  int max_bitrate_bps) {
  TargetRateConstraints msg = ConvertConstraints(
      min_bitrate_bps, max_bitrate_bps, start_bitrate_bps, clock_);
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

  NetworkRouteChange msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.constraints = ConvertConstraints(min_bitrate_bps, max_bitrate_bps,
                                      start_bitrate_bps, clock_);
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
  if (router_->last_transfer_rate().has_value()) {
    *bandwidth =
        router_->last_transfer_rate()->network_estimate.bandwidth.bps();
    return true;
  }
  return false;
}

RtcpBandwidthObserver* SendSideCongestionController::GetBandwidthObserver() {
  return this;
}

RateLimiter* SendSideCongestionController::GetRetransmissionRateLimiter() {
  return router_->retransmission_rate_limiter();
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
  // TODO(srte): This should be made less synchronous. Now it grabs a lock in
  // the pacer just for stats usage. Some kind of push interface might make
  // sense.
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
  { network_available_ = msg.network_available; }
  WaitOnTask([this, msg]() {
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
                                               int64_t max_rtt_ms) {}

int64_t SendSideCongestionController::TimeUntilNextProcess() {
  const int kMaxProcessInterval = 60 * 1000;
  if (process_interval_.IsInfinite())
    return kMaxProcessInterval;
  int64_t next_process_ms = last_process_update_ms_ + process_interval_.ms();
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
  if (router_->pacer_configured()) {
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
  WaitOnTask([this, min_send_bitrate_bps, max_padding_bitrate_bps]() {
    streams_config_.min_pacing_rate = DataRate::bps(min_send_bitrate_bps);
    streams_config_.max_padding_rate = DataRate::bps(max_padding_bitrate_bps);
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::SetPacingFactor(float pacing_factor) {
  WaitOnTask([this, pacing_factor]() {
    streams_config_.pacing_factor = pacing_factor;
    UpdateStreamsConfig();
  });
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
