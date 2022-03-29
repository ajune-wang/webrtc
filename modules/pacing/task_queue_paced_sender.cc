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
#include <cmath>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {

constexpr const char* kSlackedTaskQueuePacedSenderFieldTrial =
    "WebRTC-SlackedTaskQueuePacedSender";

void DivideAllEntries(std::map<std::string, double>* map, double divisor) {
  for (auto& entry : *map) {
    entry.second /= divisor;
  }
}

double SumOfAllEntries(const std::map<std::string, double>& map) {
  double total = 0;
  for (const auto& entry : map) {
    total += entry.second;
  }
  return total;
}

std::string Summary(const std::map<std::string, double>& map) {
  rtc::StringBuilder str;
  for (const auto& entry : map) {
    if (str.size())
      str << ", ";
    str << entry.first << ": " << static_cast<int>(std::round(entry.second))
        << "Hz";
  }
  return str.Release();
}

}  // namespace

const int TaskQueuePacedSender::kNoPacketHoldback = -1;

void TaskQueuePacedSender::WakeUpCounter::IncrementNonDelayedTaskCount(
    std::string name) {
  ++non_delayed_task_count_[std::move(name)];
  UpdateTimestamp();
}

void TaskQueuePacedSender::WakeUpCounter::IncrementDelayedTaskCount(
    std::string name) {
  ++delayed_task_count_[std::move(name)];
  UpdateTimestamp();
}

void TaskQueuePacedSender::WakeUpCounter::IncrementProbeCount() {
  ++probe_count_;
  UpdateTimestamp();
}

void TaskQueuePacedSender::WakeUpCounter::OnValueObserved(std::string name,
                                                          int value) {
  max_values_[name] = std::max(max_values_[name], value);
}

void TaskQueuePacedSender::WakeUpCounter::UpdateTimestamp() {
  int64_t now_ns = rtc::TimeNanos();
  if (prev_log_timestamp_ns_ == -1) {
    prev_log_timestamp_ns_ = now_ns;
  }
  int64_t time_elapsed_ns = now_ns - prev_log_timestamp_ns_;
  if (time_elapsed_ns < rtc::kNumNanosecsPerSec)
    return;
  double elapsed_time_s =
      time_elapsed_ns / static_cast<double>(rtc::kNumNanosecsPerSec);
  // Normalize and summate
  DivideAllEntries(&non_delayed_task_count_, elapsed_time_s);
  DivideAllEntries(&delayed_task_count_, elapsed_time_s);
  probe_count_ /= elapsed_time_s;
  int total_non_delayed_task_count =
      static_cast<int>(std::round(SumOfAllEntries(non_delayed_task_count_)));
  int total_delayed_task_count =
      static_cast<int>(std::round(SumOfAllEntries(delayed_task_count_)));
  rtc::StringBuilder max_str;
  for (const auto& pair : max_values_) {
    rtc::StringBuilder str;
    str << "- " << pair.first << ": ";
    while (str.size() < 15)
      str << " ";
    str << pair.second;
    if (max_str.size())
      max_str << "\n";
    max_str << str.str();
  }
  RTC_LOG(LS_ERROR) << "Summary\n"
                    << "* Non-delayed: " << total_non_delayed_task_count
                    << "Hz (" << Summary(non_delayed_task_count_) << ")\n"
                    << "* Delayed:     " << total_delayed_task_count << "Hz ("
                    << Summary(delayed_task_count_) << ")\n"
                    << "* Probes:      " << probe_count_ << "Hz\n"
                    << max_str.str();
  // Reset counters
  non_delayed_task_count_.clear();
  delayed_task_count_.clear();
  max_values_.clear();
  probe_count_ = 0;
  prev_log_timestamp_ns_ = now_ns;
}

TaskQueuePacedSender::TaskQueuePacedSender(
    Clock* clock,
    PacingController::PacketSender* packet_sender,
    const WebRtcKeyValueConfig& field_trials,
    TaskQueueFactory* task_queue_factory,
    TimeDelta max_hold_back_window,
    int max_hold_back_window_in_packets)
    : clock_(clock),
      allow_low_precision_(
          field_trials.IsEnabled(kSlackedTaskQueuePacedSenderFieldTrial)),
      max_hold_back_window_(allow_low_precision_
                                ? PacingController::kMinSleepTime
                                : max_hold_back_window),
      max_hold_back_window_in_packets_(
          allow_low_precision_ ? 0 : max_hold_back_window_in_packets),
      pacing_controller_(clock,
                         packet_sender,
                         field_trials,
                         PacingController::ProcessMode::kDynamic),
      next_process_time_(Timestamp::MinusInfinity()),
      is_started_(false),
      is_shutdown_(false),
      packet_size_(/*alpha=*/0.95),
      include_overhead_(false),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "TaskQueuePacedSender",
          TaskQueueFactory::Priority::NORMAL)) {
  RTC_DCHECK_GE(max_hold_back_window_, PacingController::kMinSleepTime);
  RTC_LOG(LS_ERROR) << "allow_low_precision_: " << allow_low_precision_;
}

TaskQueuePacedSender::~TaskQueuePacedSender() {
  // Post an immediate task to mark the queue as shutting down.
  // The rtc::TaskQueue destructor will wait for pending tasks to
  // complete before continuing.
  task_queue_.PostTask([&]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    is_shutdown_ = true;
  });
}

void TaskQueuePacedSender::EnsureStarted() {
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("EnsureStarted");
  }
  task_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    is_started_ = true;
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::CreateProbeCluster(DataRate bitrate,
                                              int cluster_id) {
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("CreateProbeCluster");
  }
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
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("Resume");
  }
  task_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.Resume();
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetCongested(bool congested) {
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("SetCongested");
  }
  task_queue_.PostTask([this, congested]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetCongested(congested);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetPacingRates(DataRate pacing_rate,
                                          DataRate padding_rate) {
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("SetPacingRates");
  }
  task_queue_.PostTask([this, pacing_rate, padding_rate]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetPacingRates(pacing_rate, padding_rate);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
#if RTC_TRACE_EVENTS_ENABLED
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webrtc"),
               "TaskQueuePacedSender::EnqueuePackets");
  for (auto& packet : packets) {
    TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webrtc"),
                 "TaskQueuePacedSender::EnqueuePackets::Loop",
                 "sequence_number", packet->SequenceNumber(), "rtp_timestamp",
                 packet->Timestamp());
  }
#endif

  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("EnqueuePackets");
  }
  task_queue_.PostTask([this, packets_ = std::move(packets)]() mutable {
    RTC_DCHECK_RUN_ON(&task_queue_);
    for (auto& packet : packets_) {
      size_t packet_size = packet->payload_size() + packet->padding_size();
      if (include_overhead_) {
        packet_size += packet->headers_size();
      }
      packet_size_.Apply(1, packet_size);
      RTC_DCHECK_GE(packet->capture_time(), Timestamp::Zero());
      pacing_controller_.EnqueuePacket(std::move(packet));
    }
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetAccountForAudioPackets(bool account_for_audio) {
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("SetAccountForAudioPackets");
  }
  task_queue_.PostTask([this, account_for_audio]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetAccountForAudioPackets(account_for_audio);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetIncludeOverhead() {
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("SetIncludeOverhead");
  }
  task_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    include_overhead_ = true;
    pacing_controller_.SetIncludeOverhead();
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetTransportOverhead(DataSize overhead_per_packet) {
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("SetTransportOverhead");
  }
  task_queue_.PostTask([this, overhead_per_packet]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetTransportOverhead(overhead_per_packet);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetQueueTimeLimit(TimeDelta limit) {
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementNonDelayedTaskCount("SetQueueTimeLimit");
  }
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
  Timestamp oldest_packet = GetStats().oldest_packet_enqueue_time;
  if (oldest_packet.IsInfinite()) {
    return TimeDelta::Zero();
  }

  // (webrtc:9716): The clock is not always monotonic.
  Timestamp current = clock_->CurrentTime();
  if (current < oldest_packet) {
    return TimeDelta::Zero();
  }

  return current - oldest_packet;
}

void TaskQueuePacedSender::OnStatsUpdated(const Stats& stats) {
  MutexLock lock(&stats_mutex_);
  current_stats_ = stats;
}

void TaskQueuePacedSender::MaybeProcessPackets(
    Timestamp scheduled_process_time) {
  RTC_DCHECK_RUN_ON(&task_queue_);

#if RTC_TRACE_EVENTS_ENABLED
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webrtc"),
               "TaskQueuePacedSender::MaybeProcessPackets");
#endif

  if (is_shutdown_ || !is_started_) {
    return;
  }

  Stats stats_before;
  stats_before.queue_size = pacing_controller_.QueueSizeData();
  stats_before.expected_queue_time = pacing_controller_.ExpectedQueueTime();
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.OnValueObserved("queue_size",
                                     stats_before.queue_size.bytes());
    wake_up_counter_.OnValueObserved("exp_q_time",
                                     stats_before.expected_queue_time.ms());
  }

  if (pacing_controller_.IsProbing()) {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.IncrementProbeCount();
  }

  Timestamp next_send_time = pacing_controller_.NextSendTime();
  RTC_DCHECK(next_send_time.IsFinite());
  const Timestamp now = clock_->CurrentTime();
  TimeDelta early_execute_margin =
      pacing_controller_.IsProbing()
          ? PacingController::kMaxEarlyProbeProcessing
          : TimeDelta::Zero();

  // Process packets and update stats.
  while (next_send_time <= now + early_execute_margin) {
    pacing_controller_.ProcessPackets();
    next_send_time = pacing_controller_.NextSendTime();
    RTC_DCHECK(next_send_time.IsFinite());

    // Probing state could change. Get margin after process packets.
    early_execute_margin = pacing_controller_.IsProbing()
                               ? PacingController::kMaxEarlyProbeProcessing
                               : TimeDelta::Zero();
  }
  UpdateStats();

  Stats stats_after;
  stats_after.queue_size = pacing_controller_.QueueSizeData();
  stats_after.expected_queue_time = pacing_controller_.ExpectedQueueTime();
  {
    MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
    wake_up_counter_.OnValueObserved(
        "d queue_size",
        stats_before.queue_size.bytes() - stats_after.queue_size.bytes());
    wake_up_counter_.OnValueObserved("d exp_q_time",
                                     stats_before.expected_queue_time.ms() -
                                         stats_after.expected_queue_time.ms());
  }

  // Ignore retired scheduled task, otherwise reset `next_process_time_`.
  if (scheduled_process_time.IsFinite()) {
    if (scheduled_process_time != next_process_time_) {
      return;
    }
    next_process_time_ = Timestamp::MinusInfinity();
  }

  // Do not hold back in probing.
  TimeDelta hold_back_window = TimeDelta::Zero();
  if (!pacing_controller_.IsProbing()) {
    hold_back_window = max_hold_back_window_;
    DataRate pacing_rate = pacing_controller_.pacing_rate();
    if (max_hold_back_window_in_packets_ != kNoPacketHoldback &&
        !pacing_rate.IsZero() &&
        packet_size_.filtered() != rtc::ExpFilter::kValueUndefined) {
      TimeDelta avg_packet_send_time =
          DataSize::Bytes(packet_size_.filtered()) / pacing_rate;
      hold_back_window =
          std::min(hold_back_window,
                   avg_packet_send_time * max_hold_back_window_in_packets_);
    }
  }

  // Calculate next process time.
  TimeDelta time_to_next_process =
      std::max(hold_back_window, next_send_time - now - early_execute_margin);
  next_send_time = now + time_to_next_process;

  // If no in flight task or in flight task is later than `next_send_time`,
  // schedule a new one. Previous in flight task will be retired.
  if (next_process_time_.IsMinusInfinity() ||
      next_process_time_ > next_send_time) {
    // Prefer low precision if allowed and not probing.
    TaskQueueBase::DelayPrecision precision =
        allow_low_precision_ && !pacing_controller_.IsProbing()
            ? TaskQueueBase::DelayPrecision::kLow
            : TaskQueueBase::DelayPrecision::kHigh;

    {
      MutexLock wake_up_counter_lock(&wake_up_counter_mutex_);
      if (precision == TaskQueueBase::DelayPrecision::kLow) {
        wake_up_counter_.IncrementDelayedTaskCount("PostDelayedTask");
      } else {
        wake_up_counter_.IncrementDelayedTaskCount(
            "PostDelayedHighPrecisionTask");
      }
    }

    task_queue_.PostDelayedTaskWithPrecision(
        precision,
        [this, next_send_time]() { MaybeProcessPackets(next_send_time); },
        time_to_next_process.RoundUpTo(TimeDelta::Millis(1)).ms<uint32_t>());
    next_process_time_ = next_send_time;
  }
}

void TaskQueuePacedSender::UpdateStats() {
  Stats new_stats;
  new_stats.expected_queue_time = pacing_controller_.ExpectedQueueTime();
  new_stats.first_sent_packet_time = pacing_controller_.FirstSentPacketTime();
  new_stats.oldest_packet_enqueue_time =
      pacing_controller_.OldestPacketEnqueueTime();
  new_stats.queue_size = pacing_controller_.QueueSizeData();
  OnStatsUpdated(new_stats);
}

TaskQueuePacedSender::Stats TaskQueuePacedSender::GetStats() const {
  MutexLock lock(&stats_mutex_);
  return current_stats_;
}

}  // namespace webrtc
