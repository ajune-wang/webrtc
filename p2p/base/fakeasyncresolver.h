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

#include "rtc_base/asyncinvoker.h"
#include "rtc_base/asyncresolverinterface.h"

namespace webrtc {

class FakeAsyncResolver : public rtc::AsyncResolverInterface {
 public:
  /* AsyncResolverInterface methods */
  void Start(const rtc::SocketAddress& addr) override;
  bool GetResolvedAddress(int family, rtc::SocketAddress* addr) const override;
  int GetError() const override;
  // Note that this doesn't actually do "delete this" to avoid sanitizer
  // failures caused by the synchronous nature of this implementation. The test
  // code should delete the object instead. Use MockAsyncResolver to ensure real
  // code calls Destroy.
  void Destroy(bool wait) override;

 private:
  rtc::SocketAddress addr_;
};

}  // namespace webrtc

#endif  // P2P_BASE_FAKEASYNCRESOLVER_H_
