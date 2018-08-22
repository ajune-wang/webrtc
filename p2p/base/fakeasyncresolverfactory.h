/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_FAKEASYNCRESOLVERFACTORY_H_
#define P2P_BASE_FAKEASYNCRESOLVERFACTORY_H_

#include <memory>
#include <vector>
#include "api/asyncresolverfactory.h"

namespace webrtc {

class FakeAsyncResolverFactory : public AsyncResolverFactory {
 public:
  FakeAsyncResolverFactory();
  ~FakeAsyncResolverFactory() override;

  rtc::AsyncResolverInterface* Create() override;

 private:
  // The factory manages the lifetimes of the resolvers it creates to
  // avoid problems with the synchronous implementation of FakeAsyncResolver.
  std::vector<std::unique_ptr<rtc::AsyncResolverInterface>> resolvers_;
};

}  // namespace webrtc

#endif  // P2P_BASE_FAKEASYNCRESOLVERFACTORY_H_
