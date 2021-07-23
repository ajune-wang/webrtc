/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYNCHRONIZATION_MUTEX_PTHREAD_H_
#define RTC_BASE_SYNCHRONIZATION_MUTEX_PTHREAD_H_

#if defined(WEBRTC_POSIX)

#include <pthread.h>
#if defined(WEBRTC_MAC)
#include <pthread_spis.h>
#endif

#include "absl/base/attributes.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_LOCKABLE MutexImpl final {
 public:
  MutexImpl() {
    pthread_mutexattr_t mutex_attribute;
    pthread_mutexattr_init(&mutex_attribute);
#if defined(WEBRTC_MAC)
    pthread_mutexattr_setpolicy_np(&mutex_attribute,
                                   _PTHREAD_MUTEX_POLICY_FIRSTFIT);
#endif
    pthread_mutex_init(&mutex_, &mutex_attribute);
    pthread_mutexattr_destroy(&mutex_attribute);
    owner_.store(kNoThread);
  }
  MutexImpl(const MutexImpl&) = delete;
  MutexImpl& operator=(const MutexImpl&) = delete;
  ~MutexImpl() { pthread_mutex_destroy(&mutex_); }

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION() {
    pthread_mutex_lock(&mutex_);
    owner_.store(pthread_self());
  }
  ABSL_MUST_USE_RESULT bool TryLock() RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    if (pthread_mutex_trylock(&mutex_) != 0) {
      return false;
    }
    owner_.store(pthread_self());
    return true;
  }
  void AssertHeld() const RTC_ASSERT_EXCLUSIVE_LOCK() {
    RTC_CHECK(pthread_self() == owner_.load());
  }
  void Unlock() RTC_UNLOCK_FUNCTION() {
    owner_.store(kNoThread);
    pthread_mutex_unlock(&mutex_);
  }

 private:
  // NOTE: Using a magic value for no thread is not posix portable. The
  // implementation of AssertHeld works only if pthread_t is an arithmetic type,
  // no valid thread has id zero, and std::atomic<pthread_t> is lockfree. We
  // could maybe use non-atomic variables, and accept a data race and
  // potentially unreliable behavior in the AssertHeld failure case? That seems
  // to be what chromium's AssertAcquired is doing. Abseil's AssertHeld is doing
  // something considerably more complex.
  static constexpr pthread_t kNoThread = 0;
  // Stores the owning thread, when the mutex is locked. Can be read from
  // an arbitrary thread in the case that AssertHeld fails.
  std::atomic<pthread_t> owner_;
  pthread_mutex_t mutex_;
};

}  // namespace webrtc
#endif  // #if defined(WEBRTC_POSIX)
#endif  // RTC_BASE_SYNCHRONIZATION_MUTEX_PTHREAD_H_
