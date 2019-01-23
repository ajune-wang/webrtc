/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ASYNC_RESOLVER_H_
#define RTC_BASE_ASYNC_RESOLVER_H_

#include <vector>

#include "rtc_base/asyncresolverinterface.h"
#include "rtc_base/ipaddress.h"
#include "rtc_base/signalthread.h"
#include "rtc_base/socketaddress.h"

namespace rtc {

// AsyncResolver will perform async DNS resolution, signaling the result on
// the SignalDone from AsyncResolverInterface when the operation completes.
class AsyncResolver : public AsyncResolverInterface, private SignalThread {
 public:
  AsyncResolver();
  ~AsyncResolver() override;

  void Start(const SocketAddress& addr) override;
  bool GetResolvedAddress(int family, SocketAddress* addr) const override;
  int GetError() const override;
  void Destroy(bool wait) override;

  const std::vector<IPAddress>& addresses() const { return addresses_; }
  void set_error(int error) { error_ = error; }

 protected:
  void DoWork() override;
  void OnWorkDone() override;

 private:
  SocketAddress addr_;
  std::vector<IPAddress> addresses_;
  int error_;
};

}  // namespace rtc

#endif  // RTC_BASE_ASYNC_RESOLVER_H_
