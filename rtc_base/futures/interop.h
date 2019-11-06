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
#include "absl/types/variant.h"
#include "rtc_base/futures/future.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

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
TaskHandle SpawnFutureHereImmediately(BoxedFuture<Output> future,
                                      std::function<void(Output)> callback) {
  rtc::scoped_refptr<Task> task(new rtc::RefCountedObject<CallbackTask<Output>>(
      future.Release(), std::move(callback)));
  task->Step();
  return TaskHandle{std::move(task)};
}

template <typename Output>
class AsyncCallbackFuture : public Future<Output> {
 public:
  using CompleteCB = std::function<void(Output)>;
  using StartCB = std::function<void(CompleteCB)>;

  explicit AsyncCallbackFuture(StartCB start)
      : state_(Init{std::move(start)}), weak_ptr_factory_(this) {
    RTC_DCHECK(absl::get<Init>(state_).start_cb);
  }

  // Future implementation.
  poll_t<Output> Poll(Context* context) {
    if (auto* init = absl::get_if<Init>(&state_)) {
      StartCB start_cb = std::move(init->start_cb);
      state_ = Pending{};
      auto complete_cb = [weak_ptr =
                              weak_ptr_factory_.GetWeakPtr()](Output output) {
        if (!weak_ptr) {
          return;
        }
        weak_ptr->Done(std::move(output));
      };
      start_cb([complete_cb = std::move(complete_cb),
                thread = rtc::Thread::Current()](Output output) {
        if (thread->IsCurrent()) {
          complete_cb(std::move(output));
          return;
        }
        thread->PostTask(RTC_FROM_HERE, [complete_cb = std::move(complete_cb),
                                         output = std::move(output)]() {
          complete_cb(std::move(output));
        });
      });
    }
    if (auto* pending = absl::get_if<Pending>(&state_)) {
      pending->waker = context->waker();
      return absl::nullopt;
    } else if (auto* ready = absl::get_if<Ready>(&state_)) {
      Output result = std::move(ready->result);
      state_ = Complete{};
      return absl::make_optional(std::move(result));
    } else {
      RTC_NOTREACHED() << "Poll() called after Ready returned.";
      return absl::nullopt;
    }
  }

 private:
  void Done(Output result) {
    auto* pending = absl::get_if<Pending>(&state_);
    RTC_DCHECK(pending);
    rtc::scoped_refptr<Waker> waker = std::move(pending->waker);
    state_ = Ready{std::move(result)};
    if (waker) {
      waker->WakeByRef();
    }
  }

  struct Init {
    StartCB start_cb;
  };
  struct Pending {
    rtc::scoped_refptr<Waker> waker;
  };
  struct Ready {
    Output result;
  };
  struct Complete {};

  absl::variant<Init, Pending, Ready, Complete> state_;
  rtc::WeakPtrFactory<AsyncCallbackFuture<Output>> weak_ptr_factory_;
};

}  // namespace webrtc

#endif  // RTC_BASE_FUTURES_INTEROP_H_
