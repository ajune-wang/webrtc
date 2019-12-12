/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_PEER_CONNECTION_WRAPPER_WITH_CANDIDATES_H_
#define PC_TEST_PEER_CONNECTION_WRAPPER_WITH_CANDIDATES_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "pc/peer_connection_wrapper.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "pc/test/peer_connection_test_wrapper.h"
#include "rtc_base/gunit.h"
#include "test/gtest.h"

namespace webrtc {

class PeerConnection;
class PeerConnectionWrapperWithCandidateHandler;

typedef PeerConnectionWrapperWithCandidateHandler* RawWrapperPtr;

class PeerConnectionObserverWithCandidateHandler
    : public MockPeerConnectionObserver {
 public:
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

  void PrepareToExchangeCandidates(RawWrapperPtr other) {
    candidate_target_ = other;
  }

  bool HaveDataChannel() { return last_datachannel_; }

  bool candidate_gathered() const { return candidate_gathered_; }

 private:
  bool candidate_gathered_ = false;
  RawWrapperPtr candidate_target_;  // Note: Not thread-safe against deletions.
};

class PeerConnectionWrapperWithCandidateHandler : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;
  static const int kDefaultTimeout = 10000;

  PeerConnection* GetInternalPeerConnection();

  // Override with different return type
  PeerConnectionObserverWithCandidateHandler* observer() {
    return static_cast<PeerConnectionObserverWithCandidateHandler*>(
        PeerConnectionWrapper::observer());
  }

  void PrepareToExchangeCandidates(
      PeerConnectionWrapperWithCandidateHandler* other) {
    observer()->PrepareToExchangeCandidates(other);
    other->observer()->PrepareToExchangeCandidates(this);
  }

  bool IsConnected() {
    return pc()->ice_connection_state() ==
               PeerConnectionInterface::kIceConnectionConnected ||
           pc()->ice_connection_state() ==
               PeerConnectionInterface::kIceConnectionCompleted;
  }

  bool HaveDataChannel() {
    return static_cast<PeerConnectionObserverWithCandidateHandler*>(observer())
        ->HaveDataChannel();
  }
  void BufferIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    std::string sdp;
    EXPECT_TRUE(candidate->ToString(&sdp));
    std::unique_ptr<webrtc::IceCandidateInterface> candidate_copy(
        CreateIceCandidate(candidate->sdp_mid(), candidate->sdp_mline_index(),
                           sdp, nullptr));
    buffered_candidates_.push_back(std::move(candidate_copy));
  }

  void AddBufferedIceCandidates() {
    for (const auto& candidate : buffered_candidates_) {
      EXPECT_TRUE(pc()->AddIceCandidate(candidate.get()));
    }
    buffered_candidates_.clear();
  }

  // This method performs the following actions in sequence:
  // 1. Exchange Offer and Answer.
  // 2. Exchange ICE candidates after both caller and callee complete
  // gathering.
  // 3. Wait for ICE to connect.
  //
  // This guarantees a deterministic sequence of events and also rules out the
  // occurrence of prflx candidates if the offer/answer signaling and the
  // candidate trickling race in order.
  bool ConnectTo(PeerConnectionWrapperWithCandidateHandler* callee) {
    PrepareToExchangeCandidates(callee);
    if (!ExchangeOfferAnswerWith(callee)) {
      return false;
    }
    // Wait until the gathering completes before we signal the candidate.
    WAIT(observer()->ice_gathering_complete_, kDefaultTimeout);
    WAIT(callee->observer()->ice_gathering_complete_, kDefaultTimeout);
    AddBufferedIceCandidates();
    callee->AddBufferedIceCandidates();
    WAIT(IsConnected(), kDefaultTimeout);
    WAIT(callee->IsConnected(), kDefaultTimeout);
    return IsConnected() && callee->IsConnected();
  }

 private:
  // Candidates that have been sent but not yet configured
  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
      buffered_candidates_;
};

}  // namespace webrtc

#endif  // PC_TEST_PEER_CONNECTION_WRAPPER_WITH_CANDIDATES_H_
