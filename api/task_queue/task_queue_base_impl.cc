/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_base_impl.h"

#include "rtc_base/checks.h"

namespace rtc {

// Fake ref counting: implementers of the task queue shouldn't expect task
// implementation is stored in a refererence counter pointer.
void TaskQueue::Impl::AddRef() {
  RTC_CHECK(task_queue_ == nullptr);
}

void TaskQueue::Impl::Release() {
  RTC_CHECK(false);
}

}  // namespace rtc
