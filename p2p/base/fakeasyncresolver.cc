/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/fakeasyncresolver.h"

namespace webrtc {

void FakeAsyncResolver::Start(const rtc::SocketAddress& addr) {
  addr_ = addr;
  SignalThread::Start();
}

bool FakeAsyncResolver::GetResolvedAddress(int family,
                                           rtc::SocketAddress* addr) const {
  *addr = addr_;
  if (family == AF_INET) {
    const rtc::SocketAddress fake("1.1.1.1", 5000);
    addr->SetResolvedIP(fake.ipaddr());
    return true;
  } else if (family == AF_INET6) {
    const rtc::SocketAddress fake("2:2:2:2:2:2:2:2", 5001);
    addr->SetResolvedIP(fake.ipaddr());
    return true;
  } else {
    return false;
  }
}
int FakeAsyncResolver::GetError() const {
  return 0;
}
void FakeAsyncResolver::Destroy(bool wait) {
  SignalThread::Destroy(wait);
}

/* SignalThread methods */
void FakeAsyncResolver::OnWorkDone() {
  SignalDone(this);
}

}  // namespace webrtc
