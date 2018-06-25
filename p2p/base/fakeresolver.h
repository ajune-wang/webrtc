/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_FAKERESOLVER_H_
#define P2P_BASE_FAKERESOLVER_H_

#include "rtc_base/asyncresolverinterface.h"

namespace rtc {

// We inherit from SignalThread to get the same memory management semantics as
// AsyncResolver.
class FakeResolver : public SignalThread, public AsyncResolverInterface {
 public:
  /* AsyncResolverInterface methods */
  void Start(const SocketAddress& addr) override {
    addr_ = addr;
    SignalThread::Start();
  }
  bool GetResolvedAddress(int family, SocketAddress* addr) const override {
    *addr = addr_;
    if (family == AF_INET) {
      const SocketAddress fake("1.1.1.1", 5000);
      addr->SetResolvedIP(fake.ipaddr());
      return true;
    } else if (family == AF_INET6) {
      const SocketAddress fake("2:2:2:2:2:2:2:2", 5001);
      addr->SetResolvedIP(fake.ipaddr());
      return true;
    } else {
      return false;
    }
  }
  int GetError() const override { return 0; }
  void Destroy(bool wait) override { SignalThread::Destroy(wait); }

  /* SignalThread methods */
  void DoWork() override {}
  void OnWorkDone() override { SignalDone(this); }

 private:
  SocketAddress addr_;
};

}  // namespace rtc

#endif  // P2P_BASE_FAKERESOLVER_H_
