/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/ice_gatherer.h"

#include <utility>
#include "p2p/base/port_allocator.h"

namespace cricket {

BasicIceGatherer::BasicIceGatherer(
    std::unique_ptr<PortAllocatorSession> port_allocator_session)
    : port_allocator_session_(std::move(port_allocator_session)) {}

PortAllocatorSession* BasicIceGatherer::port_allocator_session() {
  return port_allocator_session_.get();
}

}  // namespace cricket
