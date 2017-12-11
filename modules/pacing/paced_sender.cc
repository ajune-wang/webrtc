/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/paced_sender.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <vector>
#include <utility>

#include "modules/include/module_common_types.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "modules/utility/include/process_thread.h"
#include "network_control/include/network_message.h"
#include "network_control/include/network_types.h"
#include "network_control/include/network_units.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

using std::placeholders::_1;
using webrtc::network::units::Timestamp;
using webrtc::network::units::TimeDelta;
using webrtc::network::units::DataSize;
using webrtc::network::units::DataRate;

using webrtc::network::PacerConfig;
using webrtc::network::PacerState;
using webrtc::network::ProbeClusterConfig;
using webrtc::network::ProcessInterval;
using webrtc::network::signal::TaskQueueReceiver;

namespace {
// Time limit in milliseconds between packet bursts.
const int64_t kMinPacketLimitMs = 5;
const int64_t kPausedPacketIntervalMs = 500;

// Upper cap on process interval, in case process has not been called in a long
// time.
const int64_t kMaxIntervalTimeMs = 30;

template <typename T>
std::unique_ptr<TaskQueueReceiver<T>> MakeHandler(rtc::TaskQueue* queue) {
  return std::move(rtc::MakeUnique<TaskQueueReceiver<T>>(queue));
}
}  // namespace

namespace webrtc {

const int64_t PacedSender::kMaxQueueLengthMs = 2000;

PacedSender::PacedSender(const Clock* clock,
                         PacketSender* packet_sender,
                         RtcEventLog* event_log) :
    PacedSender(clock, packet_sender, event_log,
                webrtc::field_trial::IsEnabled("WebRTC-RoundRobinPacing")
                    ? rtc::MakeUnique<PacketQueue2>(clock)
                    : rtc::MakeUnique<PacketQueue>(clock)) {}

PacedSender::PacedSender(const Clock* clock,
                         PacketSender* packet_sender,
                         RtcEventLog* event_log,
                         std::unique_ptr<PacketQueue> packets)
    : task_queue_(rtc::MakeUnique<rtc::TaskQueue>("PacerQueue")),
      clock_(clock),
      packet_sender_(packet_sender),
      paused_(false),
      media_budget_(rtc::MakeUnique<IntervalBudget>(0)),
      padding_budget_(rtc::MakeUnique<IntervalBudget>(0)),
      prober_(rtc::MakeUnique<BitrateProber>(event_log)),
      pacing_bitrate_kbps_(0),
      time_last_update_us_(clock->TimeInMicroseconds()),
      first_sent_packet_ms_(-1),
      packets_(std::move(packets)),
      process_interval_ms_(kMinPacketLimitMs),
      queue_size_packets_(0),
      queue_size_bytes_(0),
      oldest_queue_time_ms_(0),
      packet_counter_(0),
      queue_time_limit(kMaxQueueLengthMs),
      account_for_audio_(false) {
  UpdateBudgetWithElapsedTime(kMinPacketLimitMs);
  rtc::TaskQueue* task_queue = task_queue_.get();
  PacerConfigReceiver = MakeHandler<PacerConfig>(task_queue);
  PacerStateReceiver = MakeHandler<PacerState>(task_queue);
  ProbeClusterConfigReceiver = MakeHandler<ProbeClusterConfig>(task_queue);
  ProcessIntervalReceiver = MakeHandler<ProcessInterval>(task_queue);
  PacketReceiver = MakeHandler<PacketQueue::Packet>(task_queue);
  ProbingStateReceiver = MakeHandler<bool>(task_queue);

  using This = PacedSender;
  using std::bind;
  PacerConfigReceiver->SetHandler(bind(&This::OnPacerConfig, this, _1));
  PacerStateReceiver->SetHandler(bind(&This::OnPacerState, this, _1));
  ProbeClusterConfigReceiver->SetHandler(
      bind(&This::OnProbeClusterConfig, this, _1));
  ProcessIntervalReceiver->SetHandler(bind(&This::OnProcessInterval, this, _1));
  PacketReceiver->SetHandler(bind(&This::OnPacket, this, _1));
  ProbingStateReceiver->SetHandler(bind(&This::OnProbingState, this, _1));

  PacerConfigJunction.Connect(PacerConfigReceiver.get());
  PacerStateJunction.Connect(PacerStateReceiver.get());
  ProbeClusterConfigJunction.Connect(ProbeClusterConfigReceiver.get());
  ProcessIntervalJunction.Connect(ProcessIntervalReceiver.get());

  PacketJunction.Connect(PacketReceiver.get());
  ProbingStateJunction.Connect(ProbingStateReceiver.get());
}

PacedSender::~PacedSender() {}

PacerConfig::Receiver* PacedSender::GetPacerConfigReceiver() {
  return PacerConfigReceiver.get();
}

PacerState::Receiver* PacedSender::GetPacerStateReceiver() {
  return PacerStateReceiver.get();
}

ProbeClusterConfig::Receiver* PacedSender::GetProbeClusterConfigReceiver() {
  return ProbeClusterConfigReceiver.get();
}

void PacedSender::CreateProbeCluster(int bitrate_bps) {
  network::ProbeClusterConfig config;
  config.target_data_rate = DataRate::bps(bitrate_bps);
  ProbeClusterConfigJunction.OnMessage(config);
}

void PacedSender::OnProbeClusterConfig(network::ProbeClusterConfig config) {
  RTC_DCHECK(task_queue_->IsCurrent());
  int64_t bitrate_bps = config.target_data_rate.bps();
  rtc::CritScope cs(&critsect_);
  prober_->CreateProbeCluster(bitrate_bps, clock_->TimeInMilliseconds());
}

void PacedSender::Pause() {
  PacerState msg;
  msg.paused = true;
  PacerStateJunction.OnMessage(msg);
}

void PacedSender::Resume() {
  PacerState msg;
  msg.paused = false;
  PacerStateJunction.OnMessage(msg);
}

void PacedSender::OnPacerState(network::PacerState msg) {
  RTC_DCHECK(task_queue_->IsCurrent());
  if (msg.paused && !paused_)
    RTC_LOG(LS_INFO) << "PacedSender paused.";
  else if (!msg.paused && paused_)
    RTC_LOG(LS_INFO) << "PacedSender resumed.";
  paused_ = msg.paused;
  packets_->SetPauseState(msg.paused, clock_->TimeInMilliseconds());
  SyncState();
}

void PacedSender::SetProbingEnabled(bool enabled) {
  ProbingStateJunction.OnMessage(enabled);
}

void PacedSender::OnProbingState(bool enabled) {
  RTC_DCHECK(task_queue_->IsCurrent());
  rtc::CritScope cs(&critsect_);
  RTC_CHECK_EQ(0, packet_counter_);
  prober_->SetEnabled(enabled);
}

void PacedSender::SetPacingRates(uint32_t pacing_rate_bps,
                                 uint32_t padding_rate_bps) {
  network::PacerConfig msg;
  msg.time_window = TimeDelta::s(1);
  msg.data_window = DataRate::bps(pacing_rate_bps) * msg.time_window;
  msg.pad_window = DataRate::bps(padding_rate_bps) * msg.time_window;
  PacerConfigJunction.OnMessage(msg);
}

void PacedSender::OnPacerConfig(network::PacerConfig msg) {
  RTC_DCHECK(task_queue_->IsCurrent());
  DataRate pacing_rate = msg.data_window / msg.time_window;
  DataRate padding_rate = msg.pad_window / msg.time_window;
  {
    rtc::CritScope cs(&critsect_);
    pacing_bitrate_kbps_ = pacing_rate.kbps();
  }
  padding_budget_->set_target_rate_kbps(padding_rate.kbps());
}

void PacedSender::InsertPacket(RtpPacketSender::Priority priority,
                               uint32_t ssrc,
                               uint16_t sequence_number,
                               int64_t capture_time_ms,
                               size_t bytes,
                               bool retransmission) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (capture_time_ms < 0)
    capture_time_ms = now_ms;
  PacketJunction.OnMessage(PacketQueue::Packet(priority, ssrc, sequence_number,
                                               capture_time_ms, now_ms, bytes,
                                               retransmission, 0));
}

void PacedSender::OnPacket(PacketQueue::Packet packet) {
  RTC_DCHECK(task_queue_->IsCurrent());
  RTC_DCHECK(pacing_bitrate_kbps_ > 0)
      << "SetPacingRate must be called before InsertPacket.";
  packet.enqueue_time_ms = clock_->TimeInMilliseconds();
  packet.enqueue_order = packet_counter_++;
  rtc::CritScope cs(&critsect_);
  prober_->OnIncomingPacket(packet.bytes);
  packets_->Push(packet);
  SyncState();
}

void PacedSender::SetAccountForAudioPackets(bool account_for_audio) {
  account_for_audio_ = account_for_audio;
}

int64_t PacedSender::ExpectedQueueTimeMs() const {
  rtc::CritScope cs(&critsect_);
  if (queue_size_bytes_ == 0)
    return 0;
  RTC_DCHECK_GT(pacing_bitrate_kbps_, 0);
  return static_cast<int64_t>(queue_size_bytes_ * 8 / pacing_bitrate_kbps_);
}

size_t PacedSender::QueueSizePackets() const {
  rtc::CritScope cs(&critsect_);
  return queue_size_packets_;
}

int64_t PacedSender::FirstSentPacketTimeMs() const {
  rtc::CritScope cs(&critsect_);
  return first_sent_packet_ms_;
}

int64_t PacedSender::QueueInMs() const {
  rtc::CritScope cs(&critsect_);
  int64_t oldest_packet = oldest_queue_time_ms_;
  if (oldest_packet == 0)
    return 0;

  return clock_->TimeInMilliseconds() - oldest_packet;
}

int64_t PacedSender::TimeUntilNextProcess() {
  int64_t elapsed_time_us = clock_->TimeInMicroseconds() - time_last_update_us_;
  int64_t elapsed_time_ms = (elapsed_time_us + 500) / 1000;

  rtc::CritScope cs(&critsect_);
  return std::max<int64_t>(process_interval_ms_ - elapsed_time_ms, 0);
}

void PacedSender::Process() {
  int64_t now_us = clock_->TimeInMicroseconds();
  int64_t elapsed_time_ms = std::min(
      kMaxIntervalTimeMs, (now_us - time_last_update_us_ + 500) / 1000);

  ProcessInterval msg;
  msg.at_time = Timestamp::us(now_us);
  msg.elapsed_time = TimeDelta::ms(elapsed_time_ms);
  ProcessIntervalJunction.OnMessage(msg);

  time_last_update_us_ = now_us;
}

void PacedSender::OnProcessInterval(network::ProcessInterval msg) {
  RTC_DCHECK(task_queue_->IsCurrent());
  int target_bitrate_kbps = pacing_bitrate_kbps_;

  if (paused_) {
    PacedPacketInfo pacing_info;
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    if (packet_counter_ == 0)
      return;
    SendPadding(1, pacing_info);
    return;
  }

  if (msg.elapsed_time.ms() > 0) {
    size_t queue_size_bytes = packets_->SizeInBytes();
    if (queue_size_bytes > 0) {
      // Assuming equal size packets and input/output rate, the average packet
      // has avg_time_left_ms left to get queue_size_bytes out of the queue, if
      // time constraint shall be met. Determine bitrate needed for that.
      packets_->UpdateQueueTime(clock_->TimeInMilliseconds());
      int64_t avg_time_left_ms = std::max<int64_t>(
          1, queue_time_limit - packets_->AverageQueueTimeMs());
      int min_bitrate_needed_kbps =
          static_cast<int>(queue_size_bytes * 8 / avg_time_left_ms);
      if (min_bitrate_needed_kbps > target_bitrate_kbps)
        target_bitrate_kbps = min_bitrate_needed_kbps;
    }

    media_budget_->set_target_rate_kbps(target_bitrate_kbps);
    UpdateBudgetWithElapsedTime(msg.elapsed_time.ms());
  }
  PacedPacketInfo pacing_info;
  size_t bytes_sent = 0;
  size_t recommended_probe_size = 0;
  bool is_probing = prober_->IsProbing();
  if (is_probing) {
    pacing_info = prober_->CurrentCluster();
    recommended_probe_size = prober_->RecommendedMinProbeSize();
  }
  while (!packets_->Empty()) {
    // Since we need to release the lock in order to send, we first pop the
    // element from the priority queue but keep it in storage, so that we can
    // reinsert it if send fails.
    const PacketQueue::Packet& packet = packets_->BeginPop();

    if (SendPacket(packet, pacing_info)) {
      // Send succeeded, remove it from the queue.
      if (first_sent_packet_ms_ == -1) {
        rtc::CritScope cs(&critsect_);
        first_sent_packet_ms_ = clock_->TimeInMilliseconds();
      }
      bytes_sent += packet.bytes;
      packets_->FinalizePop(packet);
      if (is_probing && bytes_sent > recommended_probe_size)
        break;
    } else {
      // Send failed, put it back into the queue.
      packets_->CancelPop(packet);
      break;
    }
  }

  if (packets_->Empty()) {
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    if (packet_counter_ > 0) {
      int padding_needed =
          static_cast<int>(is_probing ? (recommended_probe_size - bytes_sent)
                                      : padding_budget_->bytes_remaining());
      if (padding_needed > 0)
        bytes_sent += SendPadding(padding_needed, pacing_info);
    }
  }

  if (is_probing) {
    rtc::CritScope cs(&critsect_);
    int now_ms = clock_->TimeInMilliseconds();
    probing_send_failure_ = bytes_sent == 0;
    if (!probing_send_failure_)
      prober_->ProbeSent(now_ms, bytes_sent);
  }
  SyncState();
}

void PacedSender::SyncState() {
  rtc::CritScope cs(&critsect_);
  queue_size_bytes_ = packets_->SizeInBytes();
  queue_size_packets_ = packets_->SizeInPackets();
  oldest_queue_time_ms_ = packets_->OldestEnqueueTimeMs();

  // When paused we wake up every 500 ms to send a padding packet to ensure
  // we won't get stuck in the paused state due to no feedback being received.
  int64_t new_interval_ms =
      paused_ ? kPausedPacketIntervalMs : kMinPacketLimitMs;
  int now_ms = clock_->TimeInMilliseconds();
  int time_to_probe_ms = prober_->TimeUntilNextProbe(now_ms);
  if (time_to_probe_ms > 0 ||
      (time_to_probe_ms == 0 && !probing_send_failure_)) {
    new_interval_ms = time_to_probe_ms;
  }
  if (new_interval_ms != process_interval_ms_) {
    process_interval_ms_ = new_interval_ms;
    // Tell the process thread to call our TimeUntilNextProcess() method to
    // refresh the estimate for when to call Process().
    if (process_thread_)
      process_thread_->WakeUp(this);
  }
}

void PacedSender::Wait() {
  rtc::Event event(false, false);
  task_queue_->PostTask([&event]() { event.Set(); });
  event.Wait(rtc::Event::kForever);
}

void PacedSender::Sync(int cycles) {
  for (int i = 0; i < cycles; i++)
    Wait();  // Wait for queued action
}

void PacedSender::ProcessThreadAttached(ProcessThread* process_thread) {
  RTC_LOG(LS_INFO) << "ProcessThreadAttached 0x" << std::hex << process_thread;
  process_thread_ = process_thread;
}

bool PacedSender::SendPacket(const PacketQueue::Packet& packet,
                             const PacedPacketInfo& pacing_info) {
  RTC_DCHECK(task_queue_->IsCurrent());
  RTC_DCHECK(!paused_);
  if (media_budget_->bytes_remaining() == 0 &&
      pacing_info.probe_cluster_id == PacedPacketInfo::kNotAProbe) {
    return false;
  }
  const bool success = packet_sender_->TimeToSendPacket(
      packet.ssrc, packet.sequence_number, packet.capture_time_ms,
      packet.retransmission, pacing_info);

  if (success) {
    if (packet.priority != kHighPriority || account_for_audio_) {
      // Update media bytes sent.
      // TODO(eladalon): TimeToSendPacket() can also return |true| in some
      // situations where nothing actually ended up being sent to the network,
      // and we probably don't want to update the budget in such cases.
      // https://bugs.chromium.org/p/webrtc/issues/detail?id=8052
      UpdateBudgetWithBytesSent(packet.bytes);
    }
  }

  return success;
}

size_t PacedSender::SendPadding(size_t padding_needed,
                                const PacedPacketInfo& pacing_info) {
  RTC_DCHECK(task_queue_->IsCurrent());
  RTC_DCHECK_GT(packet_counter_, 0);
  size_t bytes_sent =
      packet_sender_->TimeToSendPadding(padding_needed, pacing_info);

  if (bytes_sent > 0) {
    UpdateBudgetWithBytesSent(bytes_sent);
  }
  return bytes_sent;
}

void PacedSender::UpdateBudgetWithElapsedTime(int64_t delta_time_ms) {
  media_budget_->IncreaseBudget(delta_time_ms);
  padding_budget_->IncreaseBudget(delta_time_ms);
}

void PacedSender::UpdateBudgetWithBytesSent(size_t bytes_sent) {
  RTC_DCHECK(task_queue_->IsCurrent());
  media_budget_->UseBudget(bytes_sent);
  padding_budget_->UseBudget(bytes_sent);
}

void PacedSender::SetQueueTimeLimit(int limit_ms) {
  queue_time_limit = limit_ms;
}

}  // namespace webrtc
