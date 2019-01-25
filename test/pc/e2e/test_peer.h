/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_TEST_PEER_H_
#define TEST_PC_E2E_TEST_PEER_H_

#include <memory>
#include <vector>

#include "pc/peer_connection_wrapper.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/network.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/api/peerconnection_quality_test_fixture.h"

namespace webrtc {
namespace test {

// Describes single participant in the call.
class TestPeer : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;
  using Params = PeerConnectionE2EQualityTestFixture::Params;
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
  using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;
  using InjectableComponents =
      PeerConnectionE2EQualityTestFixture::InjectableComponents;

  TestPeer(rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
           rtc::scoped_refptr<PeerConnectionInterface> pc,
           std::unique_ptr<MockPeerConnectionObserver> observer,
           std::unique_ptr<Params> params,
           std::unique_ptr<rtc::NetworkManager> network_manager);

  Params* params() const;

  // Adds provided |candidates| to the owned peer connection.
  bool AddIceCandidates(std::vector<const IceCandidateInterface*> candidates);

 private:
  std::unique_ptr<Params> params_;
  // Test peer will take ownership of network manager and keep it during the
  // call.
  std::unique_ptr<rtc::NetworkManager> network_manager_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_TEST_PEER_H_
