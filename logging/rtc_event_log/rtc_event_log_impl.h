/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_IMPL_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_IMPL_H_

#include <deque>
#include <memory>
#include <string>

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sequenced_task_checker.h"

namespace webrtc {

class RtcEventLogImpl final : public RtcEventLog {
 public:
  RtcEventLogImpl(std::unique_ptr<RtcEventLogEncoder> event_encoder,
                  std::unique_ptr<rtc::TaskQueue> task_queue);

  ~RtcEventLogImpl() override;

  // TODO(eladalon): We should change these name to reflect that what we're
  // actually starting/stopping is the output of the log, not the log itself.
  bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                    int64_t output_period_ms) override;
  void StopLogging() override;

  void Log(std::unique_ptr<RtcEvent> event) override;

 private:
  void LogToMemory(std::unique_ptr<RtcEvent> event) RTC_RUN_ON(task_queue_);
  void LogEventsFromMemoryToOutput() RTC_RUN_ON(task_queue_);

  void StopOutput() RTC_RUN_ON(task_queue_);

  void WriteConfigsAndHistoryToOutput(const std::string& encoded_configs,
                                      const std::string& encoded_history)
      RTC_RUN_ON(task_queue_);
  void WriteToOutput(const std::string& output_string) RTC_RUN_ON(task_queue_);

  void StopLoggingInternal() RTC_RUN_ON(task_queue_);

  void ScheduleOutput() RTC_RUN_ON(task_queue_);

  // Make sure that the event log is "managed" - created/destroyed, as well
  // as started/stopped - from the same thread/task-queue.
  rtc::SequencedTaskChecker owner_sequence_checker_;

  // History containing all past configuration events.
  std::deque<std::unique_ptr<RtcEvent>> config_history_
      RTC_GUARDED_BY(*task_queue_);

  // History containing the most recent (non-configuration) events (~10s).
  std::deque<std::unique_ptr<RtcEvent>> history_ RTC_GUARDED_BY(*task_queue_);

  size_t max_size_bytes_ RTC_GUARDED_BY(*task_queue_);
  size_t written_bytes_ RTC_GUARDED_BY(*task_queue_);

  std::unique_ptr<RtcEventLogEncoder> event_encoder_
      RTC_GUARDED_BY(*task_queue_);
  std::unique_ptr<RtcEventLogOutput> event_output_ RTC_GUARDED_BY(*task_queue_);

  size_t num_config_events_written_ RTC_GUARDED_BY(*task_queue_);
  int64_t output_period_ms_ RTC_GUARDED_BY(*task_queue_);
  int64_t last_output_ms_ RTC_GUARDED_BY(*task_queue_);
  bool output_scheduled_ RTC_GUARDED_BY(*task_queue_);

  // Since we are posting tasks bound to |this|,  it is critical that the event
  // log and it's members outlive the |task_queue_|. Keep the "task_queue_|
  // last to ensure it destructs first, or else tasks living on the queue might
  // access other members after they've been torn down.
  std::unique_ptr<rtc::TaskQueue> task_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogImpl);
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_IMPL_H_
