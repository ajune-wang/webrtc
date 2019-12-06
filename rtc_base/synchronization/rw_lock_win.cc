/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/synchronization/rw_lock_win.h"

#include "rtc_base/logging.h"

namespace webrtc {

// The Slim Reader/Writer (SRW) Lock APIs are available on all Windows versions
// since Windows Vista.
// We used to call GetProcAddress here to check if the APIs are present, but
// since these APIs are now always available we no longer need to do that.
// Another reason to avoid GetProcAddress is that it requires knowing the name
// of the DLL that hosts the APIs, and the name of the DLL is not the same in
// all Windows versions.

RWLockWin::RWLockWin() {
  InitializeSRWLock(&lock_);
}

RWLockWin* RWLockWin::Create() {
  return new RWLockWin();
}

void RWLockWin::AcquireLockExclusive() {
  AcquireSRWLockExclusive(&lock_);
}

void RWLockWin::ReleaseLockExclusive() {
  ReleaseSRWLockExclusive(&lock_);
}

void RWLockWin::AcquireLockShared() {
  AcquireSRWLockShared(&lock_);
}

void RWLockWin::ReleaseLockShared() {
  ReleaseSRWLockShared(&lock_);
}

}  // namespace webrtc
