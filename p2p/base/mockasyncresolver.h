/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_MOCKASYNCRESOLVER_H_
#define P2P_BASE_MOCKASYNCRESOLVER_H_

#include "api/asyncresolverfactory.h"
#include "p2p/base/fakeasyncresolver.h"
#include "rtc_base/asyncresolverinterface.h"
#include "test/gmock.h"

namespace rtc {

// Inherit from FakeAsyncResolver for threading semantics.
class MockAsyncResolver : public webrtc::FakeAsyncResolver {
 public:
  MockAsyncResolver() = default;
  ~MockAsyncResolver() = default;

  MOCK_CONST_METHOD2(GetResolvedAddress, bool(int family, SocketAddress* addr));
};

}  // namespace rtc

namespace webrtc {

class MockAsyncResolverFactory : public AsyncResolverFactory {
 public:
  MOCK_METHOD0(Create, rtc::AsyncResolverInterface*());
};

}  // namespace webrtc

#endif  // P2P_BASE_MOCKASYNCRESOLVER_H_
