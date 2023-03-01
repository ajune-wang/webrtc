/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_IMPL_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_IMPL_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log_output.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_factory.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RtcEventLogImpl final : public RtcEventLog {
 public:
  // The max number of events that the history can store.
  static constexpr size_t kMaxEventsInHistory = 10000;
  // The max number of events that the config history can store.
  // The config-history is supposed to be unbounded, but needs to have some
  // bound to prevent an attack via unreasonable memory use.
  static constexpr size_t kMaxEventsInConfigHistory = 1000;

  RtcEventLogImpl(
      std::unique_ptr<RtcEventLogEncoder> encoder,
      TaskQueueFactory* task_queue_factory,
      size_t max_events_in_history = kMaxEventsInHistory,
      size_t max_config_events_in_history = kMaxEventsInConfigHistory);
  RtcEventLogImpl(const RtcEventLogImpl&) = delete;
  RtcEventLogImpl& operator=(const RtcEventLogImpl&) = delete;

  ~RtcEventLogImpl() override;

  static std::unique_ptr<RtcEventLogEncoder> CreateEncoder(
      EncodingType encoding_type);

  // TODO(eladalon): We should change these name to reflect that what we're
  // actually starting/stopping is the output of the log, not the log itself.
  bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                    int64_t output_period_ms) override;
  void StopLogging() override;
  void StopLogging(std::function<void()> callback) override;

  // If a simple task executes in a very short time (non-blocking), TaskQueue
  // would be a bit more expensive than locking. It is because the TaskQueue
  // switches the thread context for task execution.
  // All events are recorded in buffers on current thread. Writing the buffers
  // to ouput is scheduled on task queue, when the buffers are full or
  // `output_period_ms_` is expired.
  void Log(std::unique_ptr<RtcEvent> event) override;

 private:
  void LogToMemory(std::unique_ptr<RtcEvent> event);
  void LogEventsToOutput(std::deque<std::unique_ptr<RtcEvent>>& history,
                         std::deque<std::unique_ptr<RtcEvent>>& config_history)
      RTC_RUN_ON(task_queue_);
  void LogEventsFromMemoryToOutput() RTC_RUN_ON(task_queue_);

  void StopOutput() RTC_RUN_ON(task_queue_);

  void WriteConfigsAndHistoryToOutput(absl::string_view encoded_configs,
                                      absl::string_view encoded_history)
      RTC_RUN_ON(task_queue_);
  void WriteToOutput(absl::string_view output_string) RTC_RUN_ON(task_queue_);

  bool ShouldOutput();
  void ScheduleOutput();

  // Max size of event history.
  const size_t max_events_in_history_;

  // Max size of config event history.
  const size_t max_config_events_in_history_;

  // History containing all past configuration events.
  std::deque<std::unique_ptr<RtcEvent>> config_history_
      RTC_GUARDED_BY(*task_queue_);

  // History containing all past configuration events. Only accessable on
  // TaskQueue.
  std::deque<std::unique_ptr<RtcEvent>> all_config_history_
      RTC_GUARDED_BY(task_queue_);

  // History containing the most recent configuration events. It might be
  // accessed on different threads.
  std::deque<std::unique_ptr<RtcEvent>> most_recent_config_history_
      RTC_GUARDED_BY(logging_mutex_);

  // History containing the most recent (non-configuration) events (~10s). It
  // might be accessed on different threads.
  std::deque<std::unique_ptr<RtcEvent>> most_recent_history_
      RTC_GUARDED_BY(logging_mutex_);

  std::unique_ptr<RtcEventLogEncoder> event_encoder_
      RTC_GUARDED_BY(*task_queue_);
  std::unique_ptr<RtcEventLogOutput> event_output_ RTC_GUARDED_BY(*task_queue_);

  absl::optional<int64_t> output_period_ms_ RTC_GUARDED_BY(logging_mutex_);
  int64_t last_output_ms_ RTC_GUARDED_BY(logging_mutex_);
  bool output_scheduled_ RTC_GUARDED_BY(logging_mutex_) = false;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker logging_state_checker_;
  bool logging_state_started_ RTC_GUARDED_BY(logging_mutex_) = false;

  // Since we are posting tasks bound to `this`,  it is critical that the event
  // log and its members outlive `task_queue_`. Keep the `task_queue_`
  // last to ensure it destructs first, or else tasks living on the queue might
  // access other members after they've been torn down.
  std::unique_ptr<rtc::TaskQueue> task_queue_;

  mutable Mutex logging_mutex_;
};

}  // namespace webrtc

#endif  //  LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_IMPL_H_
