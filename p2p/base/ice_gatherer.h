/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_GATHERER_H_
#define P2P_BASE_ICE_GATHERER_H_

#include <memory>
#include "api/ice_gatherer_interface.h"

namespace cricket {

// A simple implementation of an IceGatherer that owns the
// PortAllocatorSession. It doesn't own the PortAllocator, so make sure the
// PortAllocator lives long enough that the PortAllocatorSession won't be
// stopped too early.
class BasicIceGatherer : public webrtc::IceGathererInterface {
 public:
  explicit BasicIceGatherer(
      std::unique_ptr<PortAllocatorSession> port_allocator_session);
  PortAllocatorSession* port_allocator_session() override;

 private:
  std::unique_ptr<PortAllocatorSession> port_allocator_session_;
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_GATHERER_H_
