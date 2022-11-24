/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_INLINE_TASK_QUEUE_ADAPTER_H_
#define RTC_BASE_INLINE_TASK_QUEUE_ADAPTER_H_

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/make_ref_counted.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class InlineTaskQueueAdapter : public webrtc::TaskQueueBase {
 public:
  explicit InlineTaskQueueAdapter(
      std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue);

  void Delete() override;
  void PostTask(absl::AnyInvocable<void() &&> task) override;
  void PostDelayedTask(absl::AnyInvocable<void() &&> task,
                       TimeDelta duration) override;
  void PostDelayedHighPrecisionTask(absl::AnyInvocable<void() &&> task,
                                    TimeDelta duration) override;

  // std::enable_if is used here to make sure that calls to PostTask() with
  // std::unique_ptr<SomeClassDerivedFromQueuedTask> would not end up being
  // caught by this template.
  template <class Lambda>
  void PostTask(Lambda&& lambda) {
    if (shared_state_->TryBeginInlineExecution()) {
      CurrentTaskQueueSetter setter(this);
      std::forward<Lambda>(lambda)();
      shared_state_->EndExecution();
    } else {
      // A queue slot was already taken for us in TryBeginInlineExecution.
      base_task_queue_->PostTask(WrapperTask(std::forward<Lambda>(lambda),
                                             shared_state_, this,
                                             /*take_queue_slot=*/false));
    }
  }

 private:
  class RTC_LOCKABLE SharedState {
   public:
    bool TryBeginInlineExecution() RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true);
    void BeginExecution(bool take_queue_slot) RTC_EXCLUSIVE_LOCK_FUNCTION();
    void EndExecution() RTC_UNLOCK_FUNCTION();

   private:
    Mutex mu_;
    // Note: to simplify WrapperTask code, we allow `queue_size_` to not be
    // guaranteed to return to zero on task queue destruction. This can happen
    // when PostTask closures are deleted before execution.
    int queue_size_ RTC_GUARDED_BY(mu_) = 0;
    // Ensuring delayed and normal posted tasks run in isolation.
    Mutex task_mu_;
  };

  // Tasks that synchronizes on `SharedState::task_mu_`.
  class WrapperTask {
   public:
    WrapperTask(absl::AnyInvocable<void() &&> task,
                rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>>
                    shared_state,
                TaskQueueBase* queue,
                bool take_queue_slot);
    void operator()();

   protected:
    absl::AnyInvocable<void() &&> task_;
    rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>> shared_state_;
    TaskQueueBase* const queue_;
    const bool take_queue_slot_;
  };

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue_;
  rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>> shared_state_ =
      rtc::make_ref_counted<SharedState>();
};

}  // namespace webrtc
#endif  // RTC_BASE_INLINE_TASK_QUEUE_ADAPTER_H_
