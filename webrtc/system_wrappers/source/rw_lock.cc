/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/include/sleep.h"

#include <assert.h>
#include <stdlib.h>

#if defined(_WIN32)
#include "webrtc/system_wrappers/source/rw_lock_win.h"
#else
#include "webrtc/system_wrappers/source/rw_lock_posix.h"
#endif

namespace webrtc {

RWLockWrapper* RWLockWrapper::CreateRWLock() {
  RWLockWrapper* rw_lock_ptr;
#ifdef _WIN32
  rw_lock_ptr = RWLockWin::Create();
#else
  rw_lock_ptr = RWLockPosix::Create();
#endif
  if (rw_lock_ptr != NULL) {
    return rw_lock_ptr;
  } else {
    int msec_wait = 10 + (rand() % 90);
    SleepMs(msec_wait);
    return CreateRWLock();
  }
}

}  // namespace webrtc
