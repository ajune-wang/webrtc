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

// Utility class to perform resource reference counting.
// It uses atomic operations and memory synchronization ordering to implement
// the operations that access the counter (read the documentation inside these
// methods to understand the memory model of this class).
class RefCounter {
 public:
  explicit RefCounter(int ref_count);
  RefCounter() = delete;

  void IncRef() {
    // The caller used to own at least one reference to the resource being
    // tracked and now it owns one more.
    // Reads and writes to the resource can be safely reordered past this
    // increment in either direction.
    std::atomic_fetch_add_explicit(&ref_count_, 1, std::memory_order_relaxed);
  }

  // Returns rtc::RefCountReleaseStatus::kDroppedLastRef if this was the last
  // reference, and the resource protected by the reference counter can be
  // deleted, otherwise rtc::RefCountReleaseStatus::kOtherRefsRemained.
  rtc::RefCountReleaseStatus DecRef() {
    // The caller used to own at least one reference to the resource being
    // tracked and now it owns one less.
    // The acquire-release memory order prevents reads and writes to the
    // tracked resource from:
    // - being reordered after the decrement, which would be illegal because
    //   at least one reference must be held in order to access the resource.
    // - being reordered before the decrement, which would be illegal because
    //   the resource should not be destroyed while someone may still be using
    //   it.
    int previous_ref_count = std::atomic_fetch_sub_explicit(
        &ref_count_, 1, std::memory_order_acq_rel);
    RTC_DCHECK_GE(previous_ref_count, 1);
    // std::atomic_fetch_sub_explicit returns the value immediately preceding
    // the effects of the decrement, so if it returns 1 it means that the
    // counter it now equal to 0.
    return previous_ref_count == 1
               ? rtc::RefCountReleaseStatus::kDroppedLastRef
               : rtc::RefCountReleaseStatus::kOtherRefsRemained;
  }

  // Return true if the reference count is one, which means that the current
  // thread owns the reference (if the reference count is used in the
  // conventional way).
  bool HasOneRef() const {
    // The caller owns at least one reference to the tracked resource; if the
    // comparison is successful, we are assured that as of the atomic
    // instruction and until the caller creates a new reference, the caller
    // is the sole owner of the tracked resource.
    // The acquire memory ordering prevents accesses made after the comparison
    // from being reordered before the load which would be illegal because
    // those accesses may assume that the caller is the sole owner of the
    // resource, but does not prevent accesses to the tracked resource from
    // being reordered after the comparison which is legal because the caller
    // still owns a reference to the object.
    return std::atomic_load_explicit(&ref_count_, std::memory_order_acquire) ==
           1;
  }

 private:
  std::atomic<int> ref_count_;
};

}  // namespace webrtc_impl
}  // namespace webrtc

#endif  // RTC_BASE_REFCOUNTER_H_
