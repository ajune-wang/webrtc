/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/call_stats.h"

#include <algorithm>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

void RemoveOldReports(int64_t now, std::list<CallStats::RttTime>* reports) {
  // An rtt report is considered valid for this long.
  const int64_t kRttTimeoutMs = 1500;
  while (!reports->empty() &&
         (now - reports->front().time) > kRttTimeoutMs) {
    //printf("Removing report: now=%llu, rtt=%llu, time=%llu\n",
    //    now, reports->front().rtt, reports->front().time);
    reports->pop_front();
  }
}

int64_t GetMaxRttMs(const std::list<CallStats::RttTime>& reports) {
  if (reports.empty())
    return -1;
  int64_t max_rtt_ms = 0;
  for (const CallStats::RttTime& rtt_time : reports)
    max_rtt_ms = std::max(rtt_time.rtt, max_rtt_ms);
  return max_rtt_ms;
}

int64_t GetAvgRttMs(const std::list<CallStats::RttTime>& reports) {
  //printf("GetAvgRttMs: %lu reports\n", reports.size());
  if (reports.empty()) {
    return -1;
  }
  //printf("(");
  int64_t sum = 0;
  for (std::list<CallStats::RttTime>::const_iterator it = reports.begin();
       it != reports.end(); ++it) {
    sum += it->rtt;
    //printf("%lld+", it->rtt);
  }
  auto ret = sum / reports.size();
  //printf(")/%lu=%llu\n", reports.size(), ret);
  return ret;
}

void UpdateAvgRttMs(const std::list<CallStats::RttTime>& reports, int64_t* avg_rtt) {
  int64_t cur_rtt_ms = GetAvgRttMs(reports);
  if (cur_rtt_ms == -1) {
    // Reset.
    //printf("New avg_rtt=Reset (-1)\n");
    *avg_rtt = -1;
    return;
  }
  if (*avg_rtt == -1) {
    // Initialize.
    //printf("New avg_rtt=Initialize (%s)\n", std::to_string(cur_rtt_ms).c_str());
    *avg_rtt = cur_rtt_ms;
    return;
  }

  // Weight factor to apply to the average rtt.
  // We weigh the old average at 70% against the new average (30%).
  constexpr const float kWeightFactor = 0.3f;
  //printf("%llu * 0.7 + %llu * 0.3", *avg_rtt, cur_rtt_ms);
  *avg_rtt = *avg_rtt * (1.0f - kWeightFactor) + cur_rtt_ms * kWeightFactor;
  //printf("=%llu\n", *avg_rtt);
}
}  // namespace

// TODO(tommi): Seems like CallStats might as well just implement this
// interface.
class RtcpObserver : public RtcpRttStats {
 public:
  explicit RtcpObserver(CallStats* owner) : owner_(owner) {}
  virtual ~RtcpObserver() {}

  virtual void OnRttUpdate(int64_t rtt) {
    owner_->OnRttUpdate(rtt);
  }

  // Returns the average RTT.
  // TODO(tommi): Seems to be called from random threads.
  virtual int64_t LastProcessedRtt() const {
    return owner_->avg_rtt_ms();
  }

 private:
  CallStats* owner_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcpObserver);
};

class CallStats::TemporaryDeregistration {
public:
  TemporaryDeregistration(Module* module, ProcessThread* process_thread,
                          bool thread_running)
      : module_(module), process_thread_(process_thread) {
    if (thread_running) {
      deregistered_ = true;
      process_thread_->DeRegisterModule(module_);
    }
  }
  ~TemporaryDeregistration() {
    if (deregistered_) {
      process_thread_->RegisterModule(module_, RTC_FROM_HERE);
    }
  }

 private:
  Module* const module_;
  ProcessThread* const process_thread_;
  bool deregistered_ = false;
};


CallStats::CallStats(Clock* clock, ProcessThread* process_thread)
    : clock_(clock),
      rtcp_rtt_stats_(new RtcpObserver(this)),
      last_process_time_(clock_->TimeInMilliseconds()),
      max_rtt_ms_(-1),
      avg_rtt_ms_(-1),
      sum_avg_rtt_ms_(0),
      num_avg_rtt_(0),
      time_of_first_rtt_ms_(-1),
      process_thread_(process_thread) {
  RTC_DCHECK(process_thread_);
  process_thread_checker_.DetachFromThread();
}

CallStats::~CallStats() {
  RTC_DCHECK_RUN_ON(&construction_thread_checker_);
  RTC_DCHECK(!process_thread_running_);
  RTC_DCHECK(observers_.empty());

  UpdateHistograms();
}

int64_t debug = 0;

int64_t CallStats::TimeUntilNextProcess() {
  RTC_DCHECK_RUN_ON(&process_thread_checker_);
  int64_t now = clock_->TimeInMilliseconds();
  debug = last_process_time_ + kUpdateIntervalMs - now;
  //printf("TimeUntilNextProcess, now=%s, dbg=%s\n",
  //  std::to_string(now).c_str(), std::to_string(debug).c_str());
  return debug;
}

void CallStats::Process() {
  RTC_DCHECK_RUN_ON(&process_thread_checker_);
  int64_t now = clock_->TimeInMilliseconds();
  /*if (now < last_process_time_ + kUpdateIntervalMs) {
    //RTC_DCHECK(false) << "Why are we being called? now=" << now << " debug=" << debug;
    printf("Why are we being called? now=%s, dbg=%s\n",
      std::to_string(now).c_str(), std::to_string(debug).c_str());
    return;
  }*/

  //printf("Process, now=%s, dbg=%s\n",
  //  std::to_string(now).c_str(), std::to_string(debug).c_str());

  last_process_time_ = now;

  UpdateRttStats(now);
}

void CallStats::UpdateRttStats(int64_t now) {
  RTC_DCHECK_RUN_ON(&process_thread_checker_);

  auto avg_rtt_ms = avg_rtt_ms_;
  RemoveOldReports(now, &reports_);
  max_rtt_ms_ = GetMaxRttMs(reports_);
  UpdateAvgRttMs(reports_, &avg_rtt_ms);
  {
    rtc::CritScope lock(&crit_);
    avg_rtt_ms_ = avg_rtt_ms;
  }

  // If there is a valid rtt, update all observers with the max rtt.
  if (max_rtt_ms_ >= 0) {
    RTC_DCHECK_GE(avg_rtt_ms, 0);
    for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnRttUpdate(avg_rtt_ms, max_rtt_ms_);
    }
    // Sum for Histogram of average RTT reported over the entire call.
    sum_avg_rtt_ms_ += avg_rtt_ms;
    ++num_avg_rtt_;
  }
}

void CallStats::ProcessThreadAttached(ProcessThread* process_thread) {
  RTC_DCHECK_RUN_ON(&construction_thread_checker_);
  RTC_DCHECK(!process_thread || process_thread_ == process_thread);
  process_thread_running_ = process_thread != nullptr;
}

int64_t CallStats::avg_rtt_ms() const {
  rtc::CritScope cs(&crit_);
  return avg_rtt_ms_;
}

RtcpRttStats* CallStats::rtcp_rtt_stats() const {
  return rtcp_rtt_stats_.get();
}

void CallStats::RegisterStatsObserver(CallStatsObserver* observer) {
  RTC_DCHECK_RUN_ON(&construction_thread_checker_);
  TemporaryDeregistration deregister(
      this, process_thread_, process_thread_running_);

  bool found = false;
  for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (*it == observer) {
      found = true;
      break;
    }
  }

  if (!found)
    observers_.push_back(observer);
}

void CallStats::DeregisterStatsObserver(CallStatsObserver* observer) {
  RTC_DCHECK_RUN_ON(&construction_thread_checker_);
  TemporaryDeregistration deregister(
      this, process_thread_, process_thread_running_);

  for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (*it == observer) {
      observers_.erase(it);
      break;
    }
  }
}

void CallStats::OnRttUpdate(int64_t rtt) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  process_thread_->PostTask(rtc::NewClosure([rtt, now_ms, this]() {
    RTC_DCHECK_RUN_ON(&process_thread_checker_);
    reports_.push_back(RttTime(rtt, now_ms));
    if (time_of_first_rtt_ms_ == -1)
      time_of_first_rtt_ms_ = now_ms;

    // UpdateRttStats(now_ms);
    // Process();
    process_thread_->WakeUp(this);
  }));
#if 0
  rtc::CritScope cs(&crit_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  reports_.push_back(RttTime(rtt, now_ms));
  if (time_of_first_rtt_ms_ == -1)
    time_of_first_rtt_ms_ = now_ms;
#endif
}

void CallStats::UpdateHistograms() {
  RTC_DCHECK_RUN_ON(&construction_thread_checker_);
  RTC_DCHECK(!process_thread_running_);
  // TODO(tommi): This is called from the dtor. Can we assume that other threads
  // are not making calls into this module now?  At least assume the process
  // thread isn't attached?
  rtc::CritScope cs(&crit_);

  if (time_of_first_rtt_ms_ == -1 || num_avg_rtt_ < 1)
    return;

  int64_t elapsed_sec =
      (clock_->TimeInMilliseconds() - time_of_first_rtt_ms_) / 1000;
  if (elapsed_sec >= metrics::kMinRunTimeInSeconds) {
    int64_t avg_rtt_ms = (sum_avg_rtt_ms_ + num_avg_rtt_ / 2) / num_avg_rtt_;
    RTC_HISTOGRAM_COUNTS_10000(
        "WebRTC.Video.AverageRoundTripTimeInMilliseconds", avg_rtt_ms);
  }
}

}  // namespace webrtc
