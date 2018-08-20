/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_FAKEASYNCRESOLVER_H_
#define P2P_BASE_FAKEASYNCRESOLVER_H_

#include "rtc_base/asyncresolverinterface.h"
#include "rtc_base/signalthread.h"

namespace webrtc {

// We inherit from SignalThread to get the same memory management semantics as
// AsyncResolver.
class FakeAsyncResolver : public rtc::SignalThread,
                          public rtc::AsyncResolverInterface {
 public:
  /* AsyncResolverInterface methods */
  void Start(const rtc::SocketAddress& addr) override;
  bool GetResolvedAddress(int family, rtc::SocketAddress* addr) const override;
  int GetError() const override;
  void Destroy(bool wait) override;

  /* SignalThread methods */
  void DoWork() override {}
  void OnWorkDone() override;

 private:
  rtc::SocketAddress addr_;
};

}  // namespace webrtc

#endif  // P2P_BASE_FAKEASYNCRESOLVER_H_
