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
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class CallStatsObserver;
class RtcpRttStats;

// CallStats keeps track of statistics for a call.
class CallStats : public Module {
 public:
  friend class RtcpObserver;

  explicit CallStats(Clock* clock);
  ~CallStats();

  // Returns a RtcpRttStats to register at a statistics provider. The object
  // has the same lifetime as the CallStats instance.
  // TODO(tommi): This is const, yet returns non-const member.
  // No protection? Unclear on what thread this is called.
  // A pointer to this object seems to be cached in multiple places and used
  // from random threads.
  RtcpRttStats* rtcp_rtt_stats() const;

  // Registers/deregisters a new observer to receive statistics updates.
  // TODO(tommi): Called on a known thread?
  void RegisterStatsObserver(CallStatsObserver* observer);
  void DeregisterStatsObserver(CallStatsObserver* observer);

  // Helper struct keeping track of the time a rtt value is reported.
  struct RttTime {
    RttTime(int64_t new_rtt, int64_t rtt_time)
        : rtt(new_rtt), time(rtt_time) {}
    const int64_t rtt;
    const int64_t time;
  };

 protected:
  void OnRttUpdate(int64_t rtt);

  int64_t avg_rtt_ms() const;

 private:
  // Implements Module, to use the process thread.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // TODO(tommi): Use this to know when we're attached to the process thread?
  // Alternatively, inject that pointer via the ctor since the call_stats
  // test code, isn't using a processthread atm.
  void ProcessThreadAttached(ProcessThread* process_thread) override;

  void UpdateHistograms();

  Clock* const clock_;
  // Protecting all members.
  rtc::CriticalSection crit_;
  // Observer receiving statistics updates.
  // TODO(tommi): Make a non-ptr instance of RtcpObserver.
  std::unique_ptr<RtcpRttStats> rtcp_rtt_stats_;
  // The last time 'Process' resulted in statistic update.
  int64_t last_process_time_ RTC_GUARDED_BY(process_thread_checker_);
  // The last RTT in the statistics update (zero if there is no valid estimate).
  int64_t max_rtt_ms_ RTC_GUARDED_BY(process_thread_checker_);
  int64_t avg_rtt_ms_ RTC_GUARDED_BY(crit_);
  int64_t sum_avg_rtt_ms_ RTC_GUARDED_BY(crit_);
  int64_t num_avg_rtt_ RTC_GUARDED_BY(crit_);
  int64_t time_of_first_rtt_ms_ RTC_GUARDED_BY(crit_);

  // All Rtt reports within valid time interval, oldest first.
  // TODO(tommi): Update only on the process thread and remove locks.
  std::list<RttTime> reports_ RTC_GUARDED_BY(crit_);

  // Observers getting stats reports.
  std::list<CallStatsObserver*> observers_;

  rtc::ThreadChecker construction_thread_checker_;
  rtc::ThreadChecker process_thread_checker_;
  ProcessThread* process_thread_ RTC_GUARDED_BY(construction_thread_checker_) =
      nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(CallStats);
};

}  // namespace webrtc

#endif  // VIDEO_CALL_STATS_H_
