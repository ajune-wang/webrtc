/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/synchronization/critical_section_posix.h"

#include <pthread.h>

namespace webrtc {
namespace webrtc_critical_section_internal {

CriticalSectionImpl::CriticalSectionImpl() {
  pthread_mutexattr_t mutex_attribute;
  pthread_mutexattr_init(&mutex_attribute);
  pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_RECURSIVE);
#if defined(WEBRTC_MAC)
  pthread_mutexattr_setpolicy_np(&mutex_attribute,
                                 _PTHREAD_MUTEX_POLICY_FAIRSHARE);
#endif
  pthread_mutex_init(&mutex_, &mutex_attribute);
  pthread_mutexattr_destroy(&mutex_attribute);
}

}  // namespace webrtc_critical_section_internal
}  // namespace webrtc
