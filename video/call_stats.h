/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CALL_STATS_H_
#define VIDEO_CALL_STATS_H_

#include <list>
#include <memory>

#include "modules/include/module.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class CallStatsObserver;

// CallStats keeps track of statistics for a call.
class CallStats : public RtcpRttStats {
 public:
  // Time interval for updating the observers.
  static constexpr int64_t kDefaultUpdateIntervalMs = 1000;

  CallStats(Clock* clock,
            rtc::TaskQueue* task_queue,
            int64_t update_interval = kDefaultUpdateIntervalMs);
  ~CallStats() override;

  // Registers/deregisters a new observer to receive statistics updates.
  // Must be called from the construction thread.
  void RegisterStatsObserver(CallStatsObserver* observer) override;

  // TODO(tommi): The semantics of this method are currently that the object
  // can be deleted straight after DeregisterStatsObserver has completed.
  // This is not ideal since it requires synchronization between threads
  // (RegisterStatsObserver can complete asynchronously).
  // Figure out a way to make this function non blocking.
  void DeregisterStatsObserver(CallStatsObserver* observer) override;

  // Expose |LastProcessedRtt()| from RtcpRttStats to the public interface, as
  // it is the part of the API that is needed by direct users of CallStats.
  // TODO(tommi): Threading or lifetime guarantees are not explicit in how
  // CallStats is used as RtcpRttStats or how pointers are cached in a
  // few different places (distributed via Call). It would be good to clarify
  // from what thread/TQ calls to OnRttUpdate and LastProcessedRtt need to be
  // allowed.
  // TODO(tommi): Delete.
  int64_t LastProcessedRtt() const;

  // Exposed for tests to test histogram support.
  // Must be called on the task queue.
  // TODO(tommi): Make these protected and move to test-only subclass.
  void UpdateHistogramsForTest() { UpdateHistograms(); }
  void ProcessForTest() { Process(); }

  // Helper struct keeping track of the time a rtt value is reported.
  struct RttTime {
    RttTime(int64_t new_rtt, int64_t rtt_time)
        : rtt(new_rtt), time(rtt_time) {}
    const int64_t rtt;
    const int64_t time;
  };

 private:
  // RtcpRttStats implementation.
  void OnRttUpdate(int64_t rtt) override;

  void Process();

  // This method must only be called when the process thread is not
  // running, and from the construction thread.
  void UpdateHistograms();

  void RegisterStatsObserverOnTQ(CallStatsObserver* observer);
  void DeregisterStatsObserverOnTQ(CallStatsObserver* observer);

  Clock* const clock_;
  const int64_t update_interval_;

  // The last RTT in the statistics update (zero if there is no valid estimate).
  int64_t max_rtt_ms_ RTC_GUARDED_BY(task_queue_checker_);

  // Accessed from random threads (seemingly). Consider atomic.
  // |avg_rtt_ms_| is allowed to be read on the process thread without a lock.
  // |avg_rtt_ms_lock_| must be held elsewhere for reading.
  // |avg_rtt_ms_lock_| must be held on the process thread for writing.
  int64_t avg_rtt_ms_;

  // Protects |avg_rtt_ms_|.
  rtc::CriticalSection avg_rtt_ms_lock_;

  // |sum_avg_rtt_ms_|, |num_avg_rtt_| and |time_of_first_rtt_ms_| are only used
  // on the ProcessThread when running. When the Process Thread is not running,
  // (and only then) they can be used in UpdateHistograms(), usually called from
  // the dtor.
  int64_t sum_avg_rtt_ms_ RTC_GUARDED_BY(task_queue_checker_);
  int64_t num_avg_rtt_ RTC_GUARDED_BY(task_queue_checker_);
  int64_t time_of_first_rtt_ms_ RTC_GUARDED_BY(task_queue_checker_);

  // All Rtt reports within valid time interval, oldest first.
  std::list<RttTime> reports_ RTC_GUARDED_BY(task_queue_checker_);

  // Observers getting stats reports.
  std::list<CallStatsObserver*> observers_ RTC_GUARDED_BY(task_queue_checker_);

  rtc::ThreadChecker construction_thread_checker_;
  rtc::SequencedTaskChecker task_queue_checker_;
  rtc::TaskQueue* const task_queue_;

  class DelayedTask;
  DelayedTask* delayed_task_ RTC_GUARDED_BY(task_queue_checker_);

  RTC_DISALLOW_COPY_AND_ASSIGN(CallStats);
};

}  // namespace webrtc

#endif  // VIDEO_CALL_STATS_H_
