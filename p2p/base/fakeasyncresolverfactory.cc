/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/fakeasyncresolverfactory.h"
#include "p2p/base/fakeasyncresolver.h"

namespace webrtc {

FakeAsyncResolverFactory::FakeAsyncResolverFactory() = default;
FakeAsyncResolverFactory::~FakeAsyncResolverFactory() = default;

rtc::AsyncResolverInterface* FakeAsyncResolverFactory::Create() {
  resolvers_.emplace_back(new FakeAsyncResolver());
  return resolvers_.back().get();
}

}  // namespace webrtc
