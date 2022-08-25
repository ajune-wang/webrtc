/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/task_queue/thread_safe_pending_task_safety_flag.h"

#include <atomic>
#include <utility>

#include "api/make_ref_counted.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counter.h"

namespace webrtc {

struct ThreadSafePendingTaskSafetyFlag::Flag {
  void Release() {
    if (ref_count.DecRef() == rtc::RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
  }
  void AddRef() { ref_count.IncRef(); }

  webrtc_impl::RefCounter ref_count{0};

  // Synchronizes `SetNotAlive` and tasks running while `SetNotAlive` was
  // called. Unused if `SetNotAlive` was called while no tasks were running.
  rtc::Event unblock_set_not_alive{true, false};

  // LSB keeps canceled state.
  // Other bits contains number of tasks created by this factory that are
  // currently running.
  // Since tasks can be running on different task queues, there might be
  // more than one.
  std::atomic<uint32_t> state{0};
};

void ThreadSafePendingTaskSafetyFlag::SetNotAlive() {
  uint32_t old_state = flag_->state.fetch_or(1, std::memory_order_acq_rel);
  if ((old_state >> 1) == 0) {
    // There were no running task.
    return;
  }
  // Some tasks were running, thus need to wait until they are done.
  flag_->unblock_set_not_alive.Wait(rtc::Event::kForever);
}

absl::AnyInvocable<void() &&> ThreadSafePendingTaskSafetyFlag::WrapTask(
    absl::AnyInvocable<void() &&> task) {
  return [flag = flag_, task = std::move(task)]() mutable {
    // Increment the counter by one unless tasks were cancelled.
    // fetch_add isn't sufficient because incremental is conditional:
    // counter shouldn't be incremented when tasks are cancelled.
    uint32_t state = 0;
    while (!flag->state.compare_exchange_weak(state, state + 2,
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed)) {
      if (state & 1) {  // Cancelled.
        return;
      }
    }

    std::move(task)();
    if (flag->state.fetch_sub(2, std::memory_order_acq_rel) == 3) {
      // old_state == 3,  i.e. new_state == 1, i.e. Tasks were canceled
      // while `task` was running and current task was the last one to run.
      // Unblock SetNotAlive.
      flag->unblock_set_not_alive.Set();
    }
  };
}

ThreadSafePendingTaskSafetyFlag::ThreadSafePendingTaskSafetyFlag()
    : flag_(rtc::make_ref_counted<Flag>()) {}

ThreadSafePendingTaskSafetyFlag::~ThreadSafePendingTaskSafetyFlag() {
  // Check `CancelAll` was called, i.e. LSB is set and number of tasks still
  // running is zero.
  RTC_DCHECK_EQ(flag_->state.load(std::memory_order_relaxed), 1);
}

}  // namespace webrtc
