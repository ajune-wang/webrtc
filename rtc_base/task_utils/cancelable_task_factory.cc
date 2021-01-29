/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/cancelable_task_factory.h"

#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counter.h"
#include "rtc_base/synchronization/mutex.h"

#if RTC_DCHECK_IS_ON
#define WEBRTC_CANCELABLE_TASK_FACTORY_DETECT_DEADLOCK
#endif

namespace webrtc {

#ifdef WEBRTC_CANCELABLE_TASK_FACTORY_DETECT_DEADLOCK
namespace {
thread_local int current_thread_running_task = 0;
}  // namespace
#endif

class CancelableTaskFactory::CancelFlagTracker {
 public:
  CancelFlagTracker() = default;
  CancelFlagTracker(const CancelFlagTracker&) = delete;
  CancelFlagTracker& operator=(const CancelFlagTracker&) = delete;

  void AddRef() { ref_count_.IncRef(); }
  void Release() {
    if (ref_count_.DecRef() == rtc::RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
  }

  void MaybeRunTask(rtc::FunctionView<void()> task);
  void CancelAll();
  bool IsCanceled() {
    MutexLock lock(&mu_);
    return canceled_;
  }

 private:
  ~CancelFlagTracker() = default;

  // Instead of calling AddRef right after CancelFlagTracker is created, start
  // ref counting with 1.
  webrtc_impl::RefCounter ref_count_{1};
  // Synchronizes CancelAll and tasks running while CancelAll was called.
  // Unused if CancelAll was called while no tasks were running.
  rtc::Event unblock_cancel_all_{true, false};
  Mutex mu_;
  bool canceled_ RTC_GUARDED_BY(mu_) = false;
  // number of tasks created by this factory that are currently running.
  // Since tasks can be running on different task queues, there might be
  // more than one.
  int num_running_ RTC_GUARDED_BY(mu_) = 0;
};

void CancelableTaskFactory::CancelFlagTracker::CancelAll() {
#ifdef WEBRTC_CANCELABLE_TASK_FACTORY_DETECT_DEADLOCK
  RTC_CHECK_EQ(current_thread_running_task, 0);
#endif
  {
    MutexLock lock(&mu_);
    canceled_ = true;
    if (num_running_ == 0) {
      return;
    }
  }
  // Some tasks were running, thus need to wait until they are done.
#ifdef WEBRTC_CANCELABLE_TASK_FACTORY_DETECT_DEADLOCK
  // Assume no valid task can take 10 seconds to run.
  static constexpr int kAlmostForeverMs = 10'000;
  RTC_CHECK(unblock_cancel_all_.Wait(kAlmostForeverMs));
#else
  unblock_cancel_all_.Wait(rtc::Event::kForever);
#endif
}

void CancelableTaskFactory::CancelFlagTracker::MaybeRunTask(
    rtc::FunctionView<void()> task) {
  {
    MutexLock lock(&mu_);
    if (canceled_) {
      return;
    }
    ++num_running_;
  }
#ifdef WEBRTC_CANCELABLE_TASK_FACTORY_DETECT_DEADLOCK
  ++current_thread_running_task;
  task();
  --current_thread_running_task;
#else
  task();
#endif
  {
    MutexLock lock(&mu_);
    RTC_DCHECK_GT(num_running_, 0);
    --num_running_;
    bool need_to_unblock_cancel_all = canceled_ && num_running_ == 0;
    if (!need_to_unblock_cancel_all) {
      return;
    }
  }
  unblock_cancel_all_.Set();
}

CancelableTaskFactory::CancelableTaskFactory()
    : flag_(*new CancelFlagTracker) {}

CancelableTaskFactory::~CancelableTaskFactory() {
  RTC_DCHECK(flag_.IsCanceled());
  flag_.Release();
}

void CancelableTaskFactory::AddRef(CancelFlagTracker& flag) {
  flag.AddRef();
}

void CancelableTaskFactory::Release(CancelFlagTracker& flag) {
  flag.Release();
}

void CancelableTaskFactory::CancelAll() {
  flag_.CancelAll();
}

void CancelableTaskFactory::MaybeRunTask(CancelFlagTracker& flag,
                                         rtc::FunctionView<void()> task) {
  flag.MaybeRunTask(task);
}

}  // namespace webrtc

#ifdef WEBRTC_CANCELABLE_TASK_FACTORY_DETECT_DEADLOCK
#undef WEBRTC_CANCELABLE_TASK_FACTORY_DETECT_DEADLOCK
#endif
