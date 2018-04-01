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
#include <utility>

#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/event.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

void RemoveOldReports(int64_t now, std::list<CallStats::RttTime>* reports) {
  static constexpr const int64_t kRttTimeoutMs = 1500;
  reports->remove_if(
      [&now](CallStats::RttTime& r) { return now - r.time > kRttTimeoutMs; });
}

int64_t GetMaxRttMs(const std::list<CallStats::RttTime>& reports) {
  int64_t max_rtt_ms = -1;
  for (const CallStats::RttTime& rtt_time : reports)
    max_rtt_ms = std::max(rtt_time.rtt, max_rtt_ms);
  return max_rtt_ms;
}

int64_t GetAvgRttMs(const std::list<CallStats::RttTime>& reports) {
  RTC_DCHECK(!reports.empty());
  int64_t sum = 0;
  for (std::list<CallStats::RttTime>::const_iterator it = reports.begin();
       it != reports.end(); ++it) {
    sum += it->rtt;
  }
  return sum / reports.size();
}

int64_t GetNewAvgRttMs(const std::list<CallStats::RttTime>& reports,
                       int64_t prev_avg_rtt) {
  if (reports.empty())
    return -1;  // Reset (invalid average).

  int64_t cur_rtt_ms = GetAvgRttMs(reports);
  if (prev_avg_rtt == -1)
    return cur_rtt_ms;  // New initial average value.

  // Weight factor to apply to the average rtt.
  // We weigh the old average at 70% against the new average (30%).
  constexpr const float kWeightFactor = 0.3f;
  return prev_avg_rtt * (1.0f - kWeightFactor) + cur_rtt_ms * kWeightFactor;
}

}  // namespace

class CallStats::DelayedTask : public rtc::QueuedTask {
 public:
  DelayedTask(CallStats* call_stats, std::unique_ptr<rtc::QueuedTask> task)
      : call_stats_(call_stats), task_(std::move(task)) {}

  void Stop() {
    RTC_DCHECK(call_stats_->task_queue_->IsCurrent());
    stop_ = true;
  }

 private:
  bool Run() override {
    if (stop_)
      return true;
    task_->Run();
    call_stats_->task_queue_->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), call_stats_->update_interval_);
    return false;
  }

  CallStats* const call_stats_;
  std::unique_ptr<rtc::QueuedTask> const task_;
  bool stop_ = false;
};

CallStats::CallStats(Clock* clock,
                     rtc::TaskQueue* task_queue,
                     int64_t update_interval /*= kDefaultUpdateIntervalMs*/)
    : clock_(clock),
      update_interval_(update_interval),
      max_rtt_ms_(-1),
      avg_rtt_ms_(-1),
      sum_avg_rtt_ms_(0),
      num_avg_rtt_(0),
      time_of_first_rtt_ms_(-1),
      task_queue_(task_queue),
      delayed_task_(nullptr) {
  RTC_DCHECK(!task_queue_->IsCurrent());
  task_queue_checker_.Detach();
}

CallStats::~CallStats() {
  RTC_DCHECK_RUN_ON(&construction_thread_checker_);
  RTC_DCHECK(observers_.empty());
}

void CallStats::Process() {
  RTC_DCHECK_RUN_ON(&task_queue_checker_);
  int64_t now = clock_->TimeInMilliseconds();

  int64_t avg_rtt_ms = avg_rtt_ms_;
  RemoveOldReports(now, &reports_);
  max_rtt_ms_ = GetMaxRttMs(reports_);
  avg_rtt_ms = GetNewAvgRttMs(reports_, avg_rtt_ms);
  {
    rtc::CritScope lock(&avg_rtt_ms_lock_);
    avg_rtt_ms_ = avg_rtt_ms;
  }

  // If there is a valid rtt, update all observers with the max rtt.
  if (max_rtt_ms_ >= 0) {
    RTC_DCHECK_GE(avg_rtt_ms, 0);
    for (CallStatsObserver* observer : observers_)
      observer->OnRttUpdate(avg_rtt_ms, max_rtt_ms_);
    // Sum for Histogram of average RTT reported over the entire call.
    sum_avg_rtt_ms_ += avg_rtt_ms;
    ++num_avg_rtt_;
  }
}

void CallStats::RegisterStatsObserver(CallStatsObserver* observer) {
  RTC_DCHECK_RUN_ON(&construction_thread_checker_);
  task_queue_->PostTask([this, observer]() {
    RTC_DCHECK_RUN_ON(&task_queue_checker_);
    auto it = std::find(observers_.begin(), observers_.end(), observer);
    if (it == observers_.end())
      observers_.push_back(observer);

    if (!delayed_task_) {
      delayed_task_ =
          new DelayedTask(this, rtc::NewClosure([this]() { Process(); }));
      task_queue_->PostDelayedTask(
          std::unique_ptr<rtc::QueuedTask>(delayed_task_), update_interval_);
    }
  });
}

void CallStats::DeregisterStatsObserver(CallStatsObserver* observer) {
  RTC_DCHECK_RUN_ON(&construction_thread_checker_);
  rtc::Event event(false, false);
  task_queue_->PostTask([this, observer, &event]() {
    RTC_DCHECK_RUN_ON(&task_queue_checker_);
    observers_.remove(observer);
    if (observers_.empty()) {
      if (delayed_task_) {
        // TODO(tommi): This has the side effet that |avg_rtt_ms_| won't
        // get updated. Is that polling design necessary?
        delayed_task_->Stop();
        delayed_task_ = nullptr;
      }
      UpdateHistograms();
    }
    event.Set();
  });
  event.Wait(rtc::Event::kForever);
}

int64_t CallStats::LastProcessedRtt() const {
  // TODO(tommi): Is this method needed? RtcpRttStats provides this value
  // asynchronously, does it also need to provide it synchronously?
  // TODO(tommi): Check if this method is always called on the task queue.
  rtc::CritScope cs(&avg_rtt_ms_lock_);
  // TODO(tommi): This value only gets updated as long as there are observers
  // attached. Consider removing LastProcessedRtt from the interface.
  return avg_rtt_ms_;
}

void CallStats::OnRttUpdate(int64_t rtt) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  task_queue_->PostTask([rtt, now_ms, this]() {
    RTC_DCHECK_RUN_ON(&task_queue_checker_);
    reports_.push_back(RttTime(rtt, now_ms));
    if (time_of_first_rtt_ms_ == -1)
      time_of_first_rtt_ms_ = now_ms;

    Process();
  });
}

void CallStats::UpdateHistograms() {
  RTC_DCHECK_RUN_ON(&task_queue_checker_);

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
