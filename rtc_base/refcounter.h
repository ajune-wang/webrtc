/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_REFCOUNTER_H_
#define RTC_BASE_REFCOUNTER_H_

#include <atomic>

#include "rtc_base/refcount.h"

namespace webrtc {
namespace webrtc_impl {

class RefCounter {
 public:
  explicit RefCounter(int ref_count);
  RefCounter() = delete;

  void IncRef() {
    // TODO(mbonadei): Add comment!
    std::atomic_fetch_add_explicit(&ref_count_, 1, std::memory_order_relaxed);
  }

  // Returns rtc::RefCountReleaseStatus::kDroppedLastRef if this was the last
  // reference, and the resource protected by the reference counter can be
  // deleted, otherwise rtc::RefCountReleaseStatus::kOtherRefsRemained.
  rtc::RefCountReleaseStatus DecRef() {
    int references = std::atomic_fetch_sub_explicit(&ref_count_, 1,
                                                    std::memory_order_acq_rel);
    return references == 0 ? rtc::RefCountReleaseStatus::kDroppedLastRef
                           : rtc::RefCountReleaseStatus::kOtherRefsRemained;
  }

  // Return whether the reference count is one. If the reference count is used
  // in the conventional way, a reference count of 1 implies that the current
  // thread owns the reference and no other thread shares it. This call performs
  // the test for a reference count of one, and performs the memory barrier
  // needed for the owning thread to act on the resource protected by the
  // reference counter, knowing that it has exclusive access.
  bool HasOneRef() const {
    return std::atomic_load_explicit(&ref_count_, std::memory_order_acquire) ==
           1;
  }

 private:
  std::atomic<int> ref_count_;
};

}  // namespace webrtc_impl
}  // namespace webrtc

#endif  // RTC_BASE_REFCOUNTER_H_
