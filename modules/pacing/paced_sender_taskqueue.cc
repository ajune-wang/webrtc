/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/paced_sender_taskqueue.h"

#include <algorithm>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {
namespace {
constexpr size_t kMaxScheduledProcessCalls = 100;
constexpr TimeDelta kMinTimeBetweenStatsUpdates = TimeDelta::Millis<500>();
}  // namespace

PacedSenderTaskQueue::PacedSenderTaskQueue(
    Clock* clock,
    PacketRouter* packet_router,
    RtcEventLog* event_log,
    const WebRtcKeyValueConfig* field_trials,
    TaskQueueFactory* task_queue_factory)
    : clock_(clock),
      packet_router_(packet_router),
      probe_started_(false),
      pacing_controller_(clock,
                         static_cast<PacingController::PacketSender*>(this),
                         event_log,
                         field_trials,
                         false),
      last_stats_update_(clock_->CurrentTime()),
      shutdown_(false),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "PacedSenderTaskQueue",
          TaskQueueFactory::Priority::NORMAL)) {}

PacedSenderTaskQueue::~PacedSenderTaskQueue() {
  Shutdown();
}

void PacedSenderTaskQueue::CreateProbeCluster(DataRate bitrate,
                                              int cluster_id) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, bitrate, cluster_id]() {
      CreateProbeCluster(bitrate, cluster_id);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  pacing_controller_.CreateProbeCluster(bitrate, cluster_id);
  MaybeProcessPackets(true, absl::nullopt);
}

void PacedSenderTaskQueue::Pause() {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this]() { Pause(); });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  pacing_controller_.Pause();
}

void PacedSenderTaskQueue::Resume() {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this]() { Resume(); });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  pacing_controller_.Resume();
  MaybeProcessPackets(false, absl::nullopt);
}

void PacedSenderTaskQueue::SetCongestionWindow(
    DataSize congestion_window_size) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, congestion_window_size]() {
      SetCongestionWindow(congestion_window_size);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  bool was_congested = pacing_controller_.Congested();
  pacing_controller_.SetCongestionWindow(congestion_window_size);
  if (was_congested && !pacing_controller_.Congested()) {
    MaybeProcessPackets(false, absl::nullopt);
  }
}

void PacedSenderTaskQueue::UpdateOutstandingData(DataSize outstanding_data) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, outstanding_data]() {
      UpdateOutstandingData(outstanding_data);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  bool was_congested = pacing_controller_.Congested();
  pacing_controller_.UpdateOutstandingData(outstanding_data);
  if (was_congested && !pacing_controller_.Congested()) {
    MaybeProcessPackets(false, absl::nullopt);
  }
}

void PacedSenderTaskQueue::SetPacingRates(DataRate pacing_rate,
                                          DataRate padding_rate) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, pacing_rate, padding_rate]() {
      SetPacingRates(pacing_rate, padding_rate);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  pacing_controller_.SetPacingRates(pacing_rate, padding_rate);
  if (pacing_controller_.QueueSizePackets() > 0) {
    MaybeProcessPackets(false, absl::nullopt);
  }
}

void PacedSenderTaskQueue::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, packets_ = std::move(packets)]() mutable {
      EnqueuePackets(std::move(packets_));
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  for (auto& packet : packets) {
    pacing_controller_.EnqueuePacket(std::move(packet));
  }
  MaybeProcessPackets(false, absl::nullopt);
}

void PacedSenderTaskQueue::SetAccountForAudioPackets(bool account_for_audio) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, account_for_audio]() {
      SetAccountForAudioPackets(account_for_audio);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(&task_queue_);
  pacing_controller_.SetAccountForAudioPackets(account_for_audio);
}

void PacedSenderTaskQueue::SetQueueTimeLimit(TimeDelta limit) {
  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, limit]() { SetQueueTimeLimit(limit); });
    return;
  }
  RTC_DCHECK_RUN_ON(&task_queue_);
  pacing_controller_.SetQueueTimeLimit(limit);
}

TimeDelta PacedSenderTaskQueue::ExpectedQueueTime() const {
  return GetStats().expected_queue_time;
}

DataSize PacedSenderTaskQueue::QueueSizeData() const {
  return GetStats().queue_size;
}

absl::optional<Timestamp> PacedSenderTaskQueue::FirstSentPacketTime() const {
  return GetStats().first_sent_packet_time;
}

TimeDelta PacedSenderTaskQueue::OldestPacketWaitTime() const {
  return GetStats().oldest_packet_wait_time;
}

void PacedSenderTaskQueue::MaybeProcessPackets(
    bool is_probe,
    absl::optional<Timestamp> scheduled_runtime) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  while (scheduled_runtime.has_value() && !scheduled_process_times_.empty() &&
         scheduled_process_times_.top() <= *scheduled_runtime) {
    scheduled_process_times_.pop();
  }

  // If we're probing, only process packets when probe timer tells us to.
  if (probe_started_ && !is_probe) {
    RTC_DCHECK(!scheduled_process_times_.empty());
    return;
  }

  pacing_controller_.ProcessPackets(scheduled_runtime);

  const Timestamp now = clock_->CurrentTime();
  auto time_until_probe = pacing_controller_.TimeUntilNextProbe();
  const Timestamp next_process_time = time_until_probe.has_value()
                                          ? now + *time_until_probe
                                          : pacing_controller_.NextSendTime();
  probe_started_ = time_until_probe.has_value();

  if (IsShutdown()) {
    return;
  }

  if (probe_started_ ||  // If probing, schedule task regardless.
      scheduled_process_times_.empty() ||
      (scheduled_process_times_.top() >
           (next_process_time + PacingController::kMinSleepTime) &&
       scheduled_process_times_.size() < kMaxScheduledProcessCalls)) {
    scheduled_process_times_.push(next_process_time);
    bool is_probe = probe_started_;
    const int64_t delay_ms =
        is_probe ? time_until_probe->ms()
                 : std::max(PacingController::kMinSleepTime,
                            (next_process_time - now) + TimeDelta::us(500))
                       .ms();
    task_queue_.PostDelayedTask(
        [this, is_probe, next_process_time]() {
          RTC_DCHECK_RUN_ON(&task_queue_);
          MaybeProcessPackets(is_probe, next_process_time);
        },
        delay_ms);
  }

  if (now - last_stats_update_ >= kMinTimeBetweenStatsUpdates) {
    rtc::CritScope cs(&crit_);
    current_stats_.expected_queue_time = pacing_controller_.ExpectedQueueTime();
    current_stats_.first_sent_packet_time =
        pacing_controller_.FirstSentPacketTime();
    current_stats_.oldest_packet_wait_time =
        pacing_controller_.OldestPacketWaitTime();
    current_stats_.queue_size = pacing_controller_.QueueSizeData();
    last_stats_update_ = now;
  }
}

std::vector<std::unique_ptr<RtpPacketToSend>>
PacedSenderTaskQueue::GeneratePadding(DataSize size) {
  return packet_router_->GeneratePadding(size.bytes());
}

void PacedSenderTaskQueue::SendRtpPacket(
    std::unique_ptr<RtpPacketToSend> packet,
    const PacedPacketInfo& cluster_info) {
  packet_router_->SendPacket(std::move(packet), cluster_info);
}

PacedSenderTaskQueue::Stats PacedSenderTaskQueue::GetStats() const {
  rtc::CritScope cs(&crit_);
  return current_stats_;
}

void PacedSenderTaskQueue::Shutdown() {
  rtc::CritScope cs(&crit_);
  shutdown_ = true;
}

bool PacedSenderTaskQueue::IsShutdown() const {
  rtc::CritScope cs(&crit_);
  return shutdown_;
}

}  // namespace webrtc
