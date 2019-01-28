/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/test_peer.h"

#include <utility>

namespace webrtc {
namespace test {

TestPeer::TestPeer(
    rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
    rtc::scoped_refptr<PeerConnectionInterface> pc,
    std::unique_ptr<MockPeerConnectionObserver> observer,
    std::unique_ptr<Params> params,
    std::unique_ptr<rtc::NetworkManager> network_manager)
    : PeerConnectionWrapper::PeerConnectionWrapper(pc_factory,
                                                   pc,
                                                   std::move(observer)),
      params_(std::move(params)),
      network_manager_(std::move(network_manager)) {}

bool TestPeer::AddIceCandidates(
    rtc::ArrayView<const IceCandidateInterface*> candidates) {
  bool success = true;
  for (const auto candidate : candidates) {
    if (!pc()->AddIceCandidate(candidate)) {
      success = false;
    }
  }
  return success;
}

}  // namespace test
}  // namespace webrtc
