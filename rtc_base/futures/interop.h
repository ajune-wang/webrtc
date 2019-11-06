/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FUTURES_INTEROP_H_
#define RTC_BASE_FUTURES_INTEROP_H_

#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "rtc_base/futures/future.h"

namespace webrtc {

// template <typename Output>
// RunCallbackFuture(std::unique_ptr<Future<Output>> future, );
//
// template <typename Output>
// TaskHandle ExecuteFuture(std::unique_ptr<Future<Output>> future);

/*
template <typename Output>
class CallbackDrivenFuture final : public Future<Output> {
 public:
  void Done(Output o);

  Poll<Output> Poll(Context* context) {
  }

 private:
  absl::optional<
};
*/

class Task : public rtc::RefCountInterface {
 public:
  ~Task() = default;

  virtual void Step() = 0;
  virtual bool IsDone() const = 0;
};

class TaskWaker : public Waker {
 public:
  TaskWaker(rtc::Thread* thread, rtc::WeakPtr<Task> task)
      : thread_(thread), task_(std::move(task)) {}

  // Waker override.
  void WakeByRef() override {
    RTC_DCHECK(thread_->IsCurrent());
    Task* task = task_.get();
    if (!task || task->IsDone()) {
      return;
    }
    task->Step();
  }

 private:
  rtc::Thread* const thread_;
  rtc::WeakPtr<Task> task_;
};

template <typename Output>
class CallbackTask : public Task {
 public:
  CallbackTask(std::unique_ptr<Future<Output>> future,
               std::function<void(Output)> callback)
      : future_(std::move(future)),
        callback_(std::move(callback)),
        weak_ptr_factory_(this) {}

  // Task implementation.
  void Step() override {
    rtc::scoped_refptr<Waker> waker_(new rtc::RefCountedObject<TaskWaker>(
        rtc::Thread::Current(), weak_ptr_factory_.GetWeakPtr()));
    Context context = Context::FromWaker(std::move(waker_));
    poll_t<Output> result = future_->Poll(&context);
    if (result) {
      std::move(callback_)(std::move(result.value()));
      future_.reset();
    }
  }

  bool IsDone() const override { return !future_; }

 private:
  std::unique_ptr<Future<Output>> future_;
  std::function<void(Output)> callback_;
  rtc::WeakPtrFactory<Task> weak_ptr_factory_;
};

struct TaskHandle {
  rtc::scoped_refptr<Task> task;
};

template <typename Output>
TaskHandle SpawnFutureHereImmediately(std::unique_ptr<Future<Output>> future,
                                      std::function<void(Output)> callback) {
  rtc::scoped_refptr<Task> task(new rtc::RefCountedObject<CallbackTask<Output>>(
      std::move(future), std::move(callback)));
  task->Step();
  return TaskHandle{std::move(task)};
}

}  // namespace webrtc

#endif  // RTC_BASE_FUTURES_INTEROP_H_
