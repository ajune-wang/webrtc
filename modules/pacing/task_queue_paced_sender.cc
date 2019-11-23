/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/task_queue_paced_sender.h"

#include <algorithm>
#include <utility>
#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {
namespace {

constexpr TimeDelta kMinTimeBetweenStatsUpdates = TimeDelta::Millis<33>();
}  // namespace

TaskQueuePacedSender::TaskQueuePacedSender(
    Clock* clock,
    PacketRouter* packet_router,
    RtcEventLog* event_log,
    const WebRtcKeyValueConfig* field_trials,
    TaskQueueFactory* task_queue_factory)
    : clock_(clock),
      packet_router_(packet_router),
      pacing_controller_(clock,
                         static_cast<PacingController::PacketSender*>(this),
                         event_log,
                         field_trials,
                         PacingController::ProcessMode::kDynamic),
      next_process_time_(Timestamp::MinusInfinity()),
      last_stats_update_(Timestamp::MinusInfinity()),
      shutdown_(false),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "TaskQueuePacedSender",
          TaskQueueFactory::Priority::NORMAL)) {}

TaskQueuePacedSender::~TaskQueuePacedSender() {
  Shutdown();
}

void TaskQueuePacedSender::CreateProbeCluster(DataRate bitrate,
                                              int cluster_id) {
  task_queue_.PostTask([this, bitrate, cluster_id]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.CreateProbeCluster(bitrate, cluster_id);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::Pause() {
  task_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.Pause();
  });
}

void TaskQueuePacedSender::Resume() {
  task_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.Resume();
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetCongestionWindow(
    DataSize congestion_window_size) {
  task_queue_.PostTask([this, congestion_window_size]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetCongestionWindow(congestion_window_size);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::UpdateOutstandingData(DataSize outstanding_data) {
  if (task_queue_.IsCurrent()) {
    RTC_DCHECK_RUN_ON(&task_queue_);
    // Fast path since this can be called once per sent packet while on the
    // task queue.
    pacing_controller_.UpdateOutstandingData(outstanding_data);
  }

  task_queue_.PostTask([this, outstanding_data]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.UpdateOutstandingData(outstanding_data);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetPacingRates(DataRate pacing_rate,
                                          DataRate padding_rate) {
  task_queue_.PostTask([this, pacing_rate, padding_rate]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetPacingRates(pacing_rate, padding_rate);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  task_queue_.PostTask([this, packets_ = std::move(packets)]() mutable {
    RTC_DCHECK_RUN_ON(&task_queue_);
    for (auto& packet : packets_) {
      pacing_controller_.EnqueuePacket(std::move(packet));
    }
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetAccountForAudioPackets(bool account_for_audio) {
  task_queue_.PostTask([this, account_for_audio]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetAccountForAudioPackets(account_for_audio);
  });
}

void TaskQueuePacedSender::SetQueueTimeLimit(TimeDelta limit) {
  task_queue_.PostTask([this, limit]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetQueueTimeLimit(limit);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

TimeDelta TaskQueuePacedSender::ExpectedQueueTime() const {
  return GetStats().expected_queue_time;
}

DataSize TaskQueuePacedSender::QueueSizeData() const {
  return GetStats().queue_size;
}

absl::optional<Timestamp> TaskQueuePacedSender::FirstSentPacketTime() const {
  return GetStats().first_sent_packet_time;
}

TimeDelta TaskQueuePacedSender::OldestPacketWaitTime() const {
  return GetStats().oldest_packet_wait_time;
}

void TaskQueuePacedSender::MaybeProcessPackets(
    Timestamp scheduled_process_time) {
  RTC_DCHECK_RUN_ON(&task_queue_);

  if (IsShutdown()) {
    return;
  }

  if (scheduled_process_time.IsFinite() &&
      scheduled_process_time == next_process_time_) {
    pacing_controller_.ProcessPackets();
    next_process_time_ = Timestamp::MinusInfinity();
  }

  const Timestamp now = clock_->CurrentTime();
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

  Timestamp next_send_time = pacing_controller_.NextSendTime();
  Timestamp next_stats_time =
      pacing_controller_.QueueSizePackets() > 0
          ? last_stats_update_ + kMinTimeBetweenStatsUpdates
          : Timestamp::PlusInfinity();

  Timestamp next_process_time =
      std::max(now, std::min(next_send_time, next_stats_time));
  if (next_process_time_.IsMinusInfinity() ||
      next_process_time <=
          next_process_time_ - PacingController::kMinSleepTime) {
    next_process_time_ = next_process_time;

    uint32_t sleep_time =
        std::max(next_process_time - now, PacingController::kMinSleepTime)
            .ms<uint32_t>();
    task_queue_.PostDelayedTask(
        [this, next_process_time]() { MaybeProcessPackets(next_process_time); },
        sleep_time);
  }
}

std::vector<std::unique_ptr<RtpPacketToSend>>
TaskQueuePacedSender::GeneratePadding(DataSize size) {
  return packet_router_->GeneratePadding(size.bytes());
}

void TaskQueuePacedSender::SendRtpPacket(
    std::unique_ptr<RtpPacketToSend> packet,
    const PacedPacketInfo& cluster_info) {
  packet_router_->SendPacket(std::move(packet), cluster_info);
}

TaskQueuePacedSender::Stats TaskQueuePacedSender::GetStats() const {
  rtc::CritScope cs(&crit_);
  return current_stats_;
}

void TaskQueuePacedSender::Shutdown() {
  rtc::CritScope cs(&crit_);
  shutdown_ = true;
}

bool TaskQueuePacedSender::IsShutdown() const {
  rtc::CritScope cs(&crit_);
  return shutdown_;
}

}  // namespace webrtc
