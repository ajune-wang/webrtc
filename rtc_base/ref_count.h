/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_REF_COUNT_H_
#define RTC_BASE_REF_COUNT_H_

#include <atomic>

namespace rtc {

enum class RefCountReleaseStatus { kDroppedLastRef, kOtherRefsRemained };

}  // namespace rtc

namespace webrtc {
namespace webrtc_impl {

class RefCounter {
 public:
  explicit RefCounter(int ref_count) : ref_count_(ref_count) {}
  RefCounter() = delete;

  void IncRef() {
    // Relaxed memory order: The current thread is allowed to act on the
    // resource protected by the reference counter both before and after the
    // atomic op, so this function doesn't prevent memory access reordering.
    ref_count_.fetch_add(1, std::memory_order_relaxed);
  }

  // Returns kDroppedLastRef if this call dropped the last reference; the caller
  // should therefore free the resource protected by the reference counter.
  // Otherwise, returns kOtherRefsRemained (note that in case of multithreading,
  // some other caller may have dropped the last reference by the time this call
  // returns; all we know is that we didn't do it).
  rtc::RefCountReleaseStatus DecRef() {
    // Use release-acquire barrier to ensure all actions on the protected
    // resource are finished before the resource can be freed.
    // When ref_count_after_subtract > 0, this function require
    // std::memory_order_release part of the barrier.
    // When ref_count_after_subtract == 0, this function require
    // std::memory_order_acquire part of the barrier.
    // In addition std::memory_order_release is used for synchronization with
    // the HasOneRef function to make sure all actions on the protected resource
    // are finished before the resource is assumed to have exclusive access.
    int ref_count_after_subtract =
        ref_count_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    return ref_count_after_subtract == 0
               ? rtc::RefCountReleaseStatus::kDroppedLastRef
               : rtc::RefCountReleaseStatus::kOtherRefsRemained;
  }

  // Return whether the reference count is one. If the reference count is used
  // in the conventional way, a reference count of 1 implies that the current
  // thread owns the reference and no other thread shares it. This call performs
  // the test for a reference count of one, and performs the memory barrier
  // needed for the owning thread to act on the resource protected by the
  // reference counter, knowing that it has exclusive access.
  bool HasOneRef() const {
    // To ensure resource protected by the reference counter has exclusive
    // access, all changes to the resource before it was released by other
    // threads must be visible by current thread. That is provided by release
    // (in DecRef) and acquire (in this function) ordering.
    return ref_count_.load(std::memory_order_acquire) == 1;
  }

 private:
  std::atomic<int> ref_count_;
};

}  // namespace webrtc_impl
}  // namespace webrtc

namespace rtc {

// Refcounted objects should implement the following informal interface:
//
// void AddRef() const ;
// RefCountReleaseStatus Release() const;
//
// You may access members of a reference-counted object, including the AddRef()
// and Release() methods, only if you already own a reference to it, or if
// you're borrowing someone else's reference. (A newly created object is a
// special case: the reference count is zero on construction, and the code that
// creates the object should immediately call AddRef(), bringing the reference
// count from zero to one, e.g., by constructing an rtc::scoped_refptr).
//
// AddRef() creates a new reference to the object.
//
// Release() releases a reference to the object; the caller now has one less
// reference than before the call. Returns kDroppedLastRef if the number of
// references dropped to zero because of this (in which case the object destroys
// itself). Otherwise, returns kOtherRefsRemained, to signal that at the precise
// time the caller's reference was dropped, other references still remained (but
// if other threads own references, this may of course have changed by the time
// Release() returns).
//
// The caller of Release() must treat it in the same way as a delete operation:
// Regardless of the return value from Release(), the caller mustn't access the
// object. The object might still be alive, due to references held by other
// users of the object, but the object can go away at any time, e.g., as the
// result of another thread calling Release().
//
// Calling AddRef() and Release() manually is discouraged. It's recommended to
// use rtc::scoped_refptr to manage all pointers to reference counted objects.
// Note that rtc::scoped_refptr depends on compile-time duck-typing; formally
// implementing the below RefCountInterface is not required.

// Interfaces where refcounting is part of the public api should
// inherit this abstract interface. The implementation of these
// methods is usually provided by the RefCountedObject template class,
// applied as a leaf in the inheritance tree.
class RefCountInterface {
 public:
  void AddRef() const { ref_count_.IncRef(); }

  RefCountReleaseStatus Release() const {
    const auto status = ref_count_.DecRef();
    if (status == RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
    return status;
  }

  bool HasOneRef() const { return ref_count_.HasOneRef(); }

  // Non-public destructor, because Release() has exclusive responsibility for
  // destroying the object.
 protected:
  virtual ~RefCountInterface() = default;

 private:
  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};
};

}  // namespace rtc

#endif  // RTC_BASE_REF_COUNT_H_
