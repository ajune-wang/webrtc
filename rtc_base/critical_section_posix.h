/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CRITICAL_SECTION_POSIX_H_
#define RTC_BASE_CRITICAL_SECTION_POSIX_H_

#include <pthread.h>

namespace webrtc {
namespace webrtc_critical_section_internal {

class CriticalSectionImpl {
 public:
  CriticalSectionImpl();
  ~CriticalSectionImpl() { pthread_mutex_destroy(&mutex_); }

  void Enter() { pthread_mutex_lock(&mutex_); }
  bool TryEnter() { return pthread_mutex_trylock(&mutex_) == 0; }
  void Leave() { pthread_mutex_unlock(&mutex_); }

 private:
  pthread_mutex_t mutex_;
};

inline void Yield() {
  constexpr struct timespec ts_null = {0};
  nanosleep(&ts_null, nullptr);
}

}  // namespace webrtc_critical_section_internal
}  // namespace webrtc

#endif  // RTC_BASE_CRITICAL_SECTION_POSIX_H_
