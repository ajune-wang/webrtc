/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "network_control/include/network_message.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace network {
namespace signal {
namespace internal {
namespace {
class PendingTask : public rtc::QueuedTask {
  using closure_t = std::function<void()>;
  using token_t = rtc::scoped_refptr<PendingTaskHandler::AliveToken>;

 public:
  PendingTask(closure_t&& closure, token_t token)
      : closure_(std::forward<closure_t>(closure)), token_(token) {}
  ~PendingTask() final{};
  bool Run() final {
    if (token_->alive)
      closure_();
    return true;
  }

 private:
  std::function<void()> closure_;
  token_t token_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PendingTask);
};

class InvokedTask : public rtc::QueuedTask {
  using closure_t = std::function<void()>;
  using token_t = rtc::scoped_refptr<PendingTaskHandler::AliveToken>;

 public:
  InvokedTask(closure_t&& closure, token_t token, rtc::Event* invoked_event)
      : closure_(std::forward<closure_t>(closure)),
        token_(token),
        invoked_(invoked_event) {}
  ~InvokedTask() final{};
  bool Run() final {
    if (token_->alive)
      closure_();
    invoked_->Set();
    return true;
  }

 private:
  std::function<void()> closure_;
  token_t token_;
  rtc::Event* invoked_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(InvokedTask);
};

class QueuedInvokeResult : public InvokeResult {
 public:
  using uptr = std::unique_ptr<QueuedInvokeResult>;
  QueuedInvokeResult() : done(false, false) {}
  ~QueuedInvokeResult() final {
    const int kWaitTimeMs = 200;
    bool task_invoked = done.Wait(kWaitTimeMs);
    RTC_DCHECK(task_invoked);
  }
  rtc::Event done;
};
}  // namespace

PendingTaskHandler::PendingTaskHandler(rtc::TaskQueue* target_queue)
    : target_queue_(target_queue), token_(new AliveToken()) {}

PendingTaskHandler::~PendingTaskHandler() {
  rtc::Event done(false, false);
  target_queue_->PostTask(rtc::NewClosure([this] { token_->alive = false; },
                                          [&done] { done.Set(); }));
  done.Wait(rtc::Event::kForever);
  RTC_DCHECK(!token_->alive);
}

internal::InvokeResult::uptr PendingTaskHandler::InvokeTask(
    closure_t&& closure) {
  auto result = rtc::MakeUnique<internal::QueuedInvokeResult>();
  std::unique_ptr<InvokedTask> pending_task =
      rtc::MakeUnique<InvokedTask>(std::move(closure), token_, &result->done);
  target_queue_->PostTask(std::move(pending_task));
  return std::move(result);
}

void PendingTaskHandler::PostTask(closure_t&& closure) {
  std::unique_ptr<PendingTask> pending_task =
      rtc::MakeUnique<PendingTask>(std::move(closure), token_);
  target_queue_->PostTask(std::move(pending_task));
}

void PendingTaskHandler::PostDelayedTask(closure_t&& closure,
                                         units::TimeDelta delay) {
  std::unique_ptr<PendingTask> pending_task =
      rtc::MakeUnique<PendingTask>(std::move(closure), token_);
  target_queue_->PostDelayedTask(std::move(pending_task), delay.ms());
}
}  // namespace internal
}  // namespace signal
}  // namespace network
}  // namespace webrtc
