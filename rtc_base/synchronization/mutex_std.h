/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYNCHRONIZATION_MUTEX_STD_H_
#define RTC_BASE_SYNCHRONIZATION_MUTEX_STD_H_

#include <mutex>

#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace webrtc_impl {

class RTC_LOCKABLE MutexImpl {
 public:
  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION() { mutex_.lock(); }
  RTC_WARN_UNUSED_RESULT bool TryLock() RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return mutex_.try_lock();
  }
  void Unlock() RTC_UNLOCK_FUNCTION() { mutex_.unlock(); }

 private:
  std::mutex mutex_;
};

}  // namespace webrtc_impl
}  // namespace webrtc

#endif  // RTC_BASE_SYNCHRONIZATION_MUTEX_STD_H_
