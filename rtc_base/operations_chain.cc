/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/operations_chain.h"

#include "rtc_base/checks.h"

namespace rtc {

namespace rtc_operations_chain_internal {

CallbackHandle::CallbackHandle(scoped_refptr<OperationsChain> operations_chain)
    : operations_chain_(std::move(operations_chain)) {}

CallbackHandle::~CallbackHandle() {
  RTC_DCHECK(has_run_ || has_cancelled_);
}

void CallbackHandle::OnOperationComplete() {
  RTC_DCHECK(!has_run_ && !has_cancelled_);
#ifdef RTC_DCHECK_IS_ON
  has_run_ = true;
#endif  // RTC_DCHECK_IS_ON
  operations_chain_->OnOperationComplete();
  // We have no reason to keep the |operations_chain_| alive through reference
  // counting anymore.
  operations_chain_ = nullptr;
}

void CallbackHandle::OnOperationCancelled() {
  RTC_DCHECK(!has_run_ && !has_cancelled_);
#ifdef RTC_DCHECK_IS_ON
  has_cancelled_ = true;
#endif  // RTC_DCHECK_IS_ON
  // Operations can only be cancelled by calling
  // OperationsChain::CancelPendingOperations(), so there is no need to inform
  // the |operations_chain_| that this operation has been cancelled.
  //
  // We have no reason to keep the |operations_chain_| alive through reference
  // counting anymore.
  operations_chain_ = nullptr;
}

}  // namespace rtc_operations_chain_internal

// static
scoped_refptr<OperationsChain> OperationsChain::Create() {
  return new OperationsChain();
}

OperationsChain::OperationsChain() : RefCountedObject() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}

OperationsChain::~OperationsChain() {
  // Operations keep the chain alive through reference counting so this should
  // not be possible. The fact that the chain is empty makes it safe to
  // destroy the OperationsChain on any sequence.
  RTC_DCHECK(chained_operations_.empty());
}

void OperationsChain::CancelPendingOperations() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // The first element on the chain, if one exists, is the operation that is
  // currently executing and thus cannot be cancelled. The first element to
  // cancel is the second in the list.
  auto first_cancelled_operation = chained_operations_.begin();
  if (first_cancelled_operation != chained_operations_.end())
    ++first_cancelled_operation;
  // Cancel all operations, in-order.
  for (auto it = first_cancelled_operation; it != chained_operations_.end();
       ++it) {
    (*it)->Cancel();
    it->reset();
  }
  chained_operations_.erase(first_cancelled_operation,
                            chained_operations_.end());
}

void OperationsChain::OnOperationComplete() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // The front element is the operation that just completed, remove it.
  RTC_DCHECK(!chained_operations_.empty());
  chained_operations_.pop_front();
  // If there are any other operations chained, execute the next one.
  if (!chained_operations_.empty()) {
    chained_operations_.front()->Run();
  }
}

}  // namespace rtc
