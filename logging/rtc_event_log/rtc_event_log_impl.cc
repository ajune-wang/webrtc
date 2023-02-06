/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_impl.h"

#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_new_format.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

std::unique_ptr<RtcEventLogEncoder> RtcEventLogImpl::CreateEncoder(
    RtcEventLog::EncodingType type) {
  switch (type) {
    case RtcEventLog::EncodingType::Legacy:
      RTC_DLOG(LS_INFO) << "Creating legacy encoder for RTC event log.";
      return std::make_unique<RtcEventLogEncoderLegacy>();
    case RtcEventLog::EncodingType::NewFormat:
      RTC_DLOG(LS_INFO) << "Creating new format encoder for RTC event log.";
      return std::make_unique<RtcEventLogEncoderNewFormat>();
    default:
      RTC_LOG(LS_ERROR) << "Unknown RtcEventLog encoder type (" << int(type)
                        << ")";
      RTC_DCHECK_NOTREACHED();
      return std::unique_ptr<RtcEventLogEncoder>(nullptr);
  }
}

RtcEventLogImpl::RtcEventLogImpl(std::unique_ptr<RtcEventLogEncoder> encoder,
                                 TaskQueueFactory* task_queue_factory,
                                 size_t max_events_in_history,
                                 size_t max_config_events_in_history)
    : max_events_in_history_(max_events_in_history),
      max_config_events_in_history_(max_config_events_in_history),
      event_encoder_(std::move(encoder)),
      last_output_ms_(rtc::TimeMillis()),
      task_queue_(
          std::make_unique<rtc::TaskQueue>(task_queue_factory->CreateTaskQueue(
              "rtc_event_log",
              TaskQueueFactory::Priority::NORMAL))) {}

RtcEventLogImpl::~RtcEventLogImpl() {
  // If we're logging to the output, this will stop that. Blocking function.
  logging_mutex_.Lock();
  bool started = logging_state_started_;
  logging_mutex_.Unlock();

  if (started) {
    logging_state_checker_.Detach();
    StopLogging();
  }

  // We want to block on any executing task by invoking ~TaskQueue() before
  // we set unique_ptr's internal pointer to null.
  rtc::TaskQueue* tq = task_queue_.get();
  delete tq;
  task_queue_.release();
}

bool RtcEventLogImpl::StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                                   int64_t output_period_ms) {
  RTC_DCHECK(output);
  RTC_DCHECK(output_period_ms == kImmediateOutput || output_period_ms > 0);

  if (!output->IsActive()) {
    // TODO(eladalon): We may want to remove the IsActive method. Otherwise
    // we probably want to be consistent and terminate any existing output.
    return false;
  }

  const int64_t timestamp_us = rtc::TimeMillis() * 1000;
  const int64_t utc_time_us = rtc::TimeUTCMillis() * 1000;
  RTC_LOG(LS_INFO) << "Starting WebRTC event log. (Timestamp, UTC) = ("
                   << timestamp_us << ", " << utc_time_us << ").";

  RTC_DCHECK_RUN_ON(&logging_state_checker_);
  MutexLock lock(&logging_mutex_);
  logging_state_started_ = true;
  output_period_ms_ = output_period_ms;

  // Binding to `this` is safe because `this` outlives the `task_queue_`.
  task_queue_->PostTask([this, timestamp_us, utc_time_us,
                         output = std::move(output)]() mutable {
    RTC_DCHECK_RUN_ON(task_queue_.get());
    RTC_DCHECK(output);
    RTC_DCHECK(output->IsActive());
    event_output_ = std::move(output);

    WriteToOutput(event_encoder_->EncodeLogStart(timestamp_us, utc_time_us));
    {
      MutexLock lock(&logging_mutex_);

      // Load all configs of previous sessions to output.
      most_recent_config_history_.insert(
          most_recent_config_history_.begin(),
          std::make_move_iterator(all_config_history_.begin()),
          std::make_move_iterator(all_config_history_.end()));
      RTC_DCHECK_LE(most_recent_config_history_.size(),
                    kMaxEventsInConfigHistory);
      all_config_history_.clear();
    }
    LogEventsFromMemoryToOutput();
  });

  return true;
}

void RtcEventLogImpl::StopLogging() {
  RTC_DLOG(LS_INFO) << "Stopping WebRTC event log.";
  // TODO(danilchap): Do not block current thread waiting on the task queue.
  // It might work for now, for current callers, but disallows caller to share
  // threads with the `task_queue_`.
  rtc::Event output_stopped;
  StopLogging([&output_stopped]() { output_stopped.Set(); });
  output_stopped.Wait(rtc::Event::kForever);

  RTC_DLOG(LS_INFO) << "WebRTC event log successfully stopped.";
}

void RtcEventLogImpl::StopLogging(std::function<void()> callback) {
  RTC_DCHECK_RUN_ON(&logging_state_checker_);
  MutexLock lock(&logging_mutex_);
  logging_state_started_ = false;
  task_queue_->PostTask([this, callback] {
    RTC_DCHECK_RUN_ON(task_queue_.get());
    if (event_output_) {
      RTC_DCHECK(event_output_->IsActive());
      LogEventsFromMemoryToOutput();
      const int64_t timestamp_us = rtc::TimeMillis() * 1000;
      WriteToOutput(event_encoder_->EncodeLogEnd(timestamp_us));
      StopOutput();
    }
    callback();
  });
}

void RtcEventLogImpl::Log(std::unique_ptr<RtcEvent> event) {
  MutexLock lock(&logging_mutex_);

  RTC_CHECK(event);
  LogToMemory(std::move(event));
  if (logging_state_started_) {
    if (ShouldOutput()) {
      // Binding to `this` is safe because `this` outlives the `task_queue_`.
      task_queue_->PostTask(
          [this, history = std::move(most_recent_history_),
           config_history = std::move(most_recent_config_history_)]() mutable {
            RTC_DCHECK_RUN_ON(task_queue_.get());
            RTC_DCHECK(event_output_);
            LogEventsToOutput(history, config_history);
          });
      most_recent_history_.clear();
      most_recent_config_history_.clear();
    } else {
      ScheduleOutput();
    }
  }
}

bool RtcEventLogImpl::ShouldOutput() {
  logging_mutex_.AssertHeld();

  if (most_recent_history_.size() >= max_events_in_history_) {
    // We have to emergency drain the buffer. We can't wait for the scheduled
    // output task because there might be other event incoming before that.
    return true;
  }

  RTC_DCHECK(output_period_ms_.has_value());
  if (*output_period_ms_ == kImmediateOutput) {
    return true;
  }

  return false;
}

void RtcEventLogImpl::ScheduleOutput() {
  logging_mutex_.AssertHeld();

  RTC_DCHECK(output_period_ms_.has_value());
  RTC_DCHECK(*output_period_ms_ != kImmediateOutput);
  if (!output_scheduled_) {
    output_scheduled_ = true;
    // Binding to `this` is safe because `this` outlives the `task_queue_`.
    auto output_task = [this]() {
      RTC_DCHECK_RUN_ON(task_queue_.get());
      if (event_output_) {
        RTC_DCHECK(event_output_->IsActive());
        LogEventsFromMemoryToOutput();
      }

      {
        MutexLock lock(&logging_mutex_);
        output_scheduled_ = false;
      }
    };
    const int64_t now_ms = rtc::TimeMillis();
    const int64_t time_since_output_ms = now_ms - last_output_ms_;
    const uint32_t delay = rtc::SafeClamp(
        *output_period_ms_ - time_since_output_ms, 0, *output_period_ms_);
    task_queue_->PostDelayedTask(std::move(output_task),
                                 TimeDelta::Millis(delay));
  }
}

void RtcEventLogImpl::LogToMemory(std::unique_ptr<RtcEvent> event) {
  logging_mutex_.AssertHeld();

  std::deque<std::unique_ptr<RtcEvent>>& container =
      event->IsConfigEvent() ? most_recent_config_history_
                             : most_recent_history_;
  const size_t container_max_size = event->IsConfigEvent()
                                        ? max_config_events_in_history_
                                        : max_events_in_history_;

  if (container.size() >= container_max_size) {
    RTC_DCHECK(!logging_state_started_);  // Shouldn't lose events if started.
    container.pop_front();
  }
  container.push_back(std::move(event));
}

void RtcEventLogImpl::LogEventsFromMemoryToOutput() {
  RTC_DCHECK_RUN_ON(task_queue_.get());

  std::deque<std::unique_ptr<RtcEvent>> history;
  std::deque<std::unique_ptr<RtcEvent>> config_history;
  {
    MutexLock lock(&logging_mutex_);
    std::swap(history, most_recent_history_);
    std::swap(config_history, most_recent_config_history_);
  }
  LogEventsToOutput(history, config_history);
}

void RtcEventLogImpl::LogEventsToOutput(
    std::deque<std::unique_ptr<RtcEvent>>& history,
    std::deque<std::unique_ptr<RtcEvent>>& config_history) {
  RTC_DCHECK_RUN_ON(task_queue_.get());
  RTC_DCHECK(event_output_ && event_output_->IsActive());

  {
    MutexLock lock(&logging_mutex_);
    last_output_ms_ = rtc::TimeMillis();
  }

  // Serialize the stream configurations.
  std::string encoded_configs =
      event_encoder_->EncodeBatch(config_history.begin(), config_history.end());

  // Serialize the events in the event queue. Note that the write may fail,
  // for example if we are writing to a file and have reached the maximum limit.
  // We don't get any feedback if this happens, so we still remove the events
  // from the event log history. This is normally not a problem, but if another
  // log is started immediately after the first one becomes full, then one
  // cannot rely on the second log to contain everything that isn't in the first
  // log; one batch of events might be missing.
  std::string encoded_history =
      event_encoder_->EncodeBatch(history.begin(), history.end());

  WriteConfigsAndHistoryToOutput(encoded_configs, encoded_history);

  // Unlike other events, the configs are retained. If we stop/start logging
  // again, these configs are used to interpret other events.
  all_config_history_.insert(all_config_history_.end(),
                             std::make_move_iterator(config_history.begin()),
                             std::make_move_iterator(config_history.end()));
  RTC_DCHECK_LE(all_config_history_.size(), kMaxEventsInConfigHistory);
}

void RtcEventLogImpl::WriteConfigsAndHistoryToOutput(
    absl::string_view encoded_configs,
    absl::string_view encoded_history) {
  // This function is used to merge the strings instead of calling the output
  // object twice with small strings. The function also avoids copying any
  // strings in the typical case where there are no config events.
  if (encoded_configs.empty()) {
    WriteToOutput(encoded_history);  // Typical case.
  } else if (encoded_history.empty()) {
    WriteToOutput(encoded_configs);  // Very unusual case.
  } else {
    std::string s;
    s.reserve(encoded_configs.size() + encoded_history.size());
    s.append(encoded_configs.data(), encoded_configs.size());
    s.append(encoded_history.data(), encoded_history.size());
    WriteToOutput(s);
  }
}

void RtcEventLogImpl::StopOutput() {
  event_output_.reset();
}

void RtcEventLogImpl::WriteToOutput(absl::string_view output_string) {
  RTC_DCHECK(event_output_ && event_output_->IsActive());
  if (!event_output_->Write(output_string)) {
    RTC_LOG(LS_ERROR) << "Failed to write RTC event to output.";
    // The first failure closes the output.
    RTC_DCHECK(!event_output_->IsActive());
    StopOutput();  // Clean-up.
    return;
  }
}

}  // namespace webrtc
