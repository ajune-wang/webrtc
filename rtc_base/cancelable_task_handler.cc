/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cancelable_task_handler.h"

#include <atomic>

#include "rtc_base/refcounter.h"

namespace rtc {

class CancelableTaskHandler::CancelationToken {
 public:
  CancelationToken() : canceled_(false), ref_count_(0) {}
  CancelationToken(const CancelationToken&) = delete;
  CancelationToken& operator=(const CancelationToken&) = delete;

  void Cancel() { canceled_.store(true, std::memory_order_release); }

  bool Canceled() { return canceled_.load(std::memory_order_acquire); }

  void AddRef() { ref_count_.IncRef(); }

  void Release() {
    if (ref_count_.DecRef() == rtc::RefCountReleaseStatus::kDroppedLastRef)
      delete this;
  }

 private:
  ~CancelationToken() = default;

  std::atomic<bool> canceled_;
  webrtc::webrtc_impl::RefCounter ref_count_;
};

CancelableTaskHandler::BaseTask::BaseTask() = default;
CancelableTaskHandler::BaseTask::~BaseTask() = default;

void CancelableTaskHandler::BaseTask::Construct(
    rtc::TaskQueue* task_queue,
    const rtc::scoped_refptr<CancelationToken>& cancelation_token) {
  task_queue_ = task_queue;
  cancelation_token_ = cancelation_token;
}

bool CancelableTaskHandler::BaseTask::Canceled() {
  return cancelation_token_->Canceled();
}

CancelableTaskHandler::CancelableTaskHandler() = default;
CancelableTaskHandler::CancelableTaskHandler(const CancelableTaskHandler&) =
    default;
CancelableTaskHandler::CancelableTaskHandler(CancelableTaskHandler&&) = default;
CancelableTaskHandler& CancelableTaskHandler::operator=(
    const CancelableTaskHandler&) = default;
CancelableTaskHandler& CancelableTaskHandler::operator=(
    CancelableTaskHandler&&) = default;
CancelableTaskHandler::~CancelableTaskHandler() = default;

void CancelableTaskHandler::StartPeriodicTaskInternal(
    std::unique_ptr<BaseTask> task,
    rtc::TaskQueue* task_queue,
    int initial_delay_ms) {
  cancelation_token_ = new CancelationToken;
  task->Construct(task_queue, cancelation_token_);
  if (initial_delay_ms > 0) {
    task_queue->PostDelayedTask(std::move(task), initial_delay_ms);
  } else {
    task_queue->PostTask(std::move(task));
  }
}

void CancelableTaskHandler::Cancel() {
  if (cancelation_token_.get() != nullptr)
    cancelation_token_->Cancel();
}

}  // namespace rtc
