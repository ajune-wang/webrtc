/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/test/peer_connection_wrapper_with_candidates.h"

#include "api/peer_connection_proxy.h"
#include "pc/peer_connection.h"

namespace webrtc {

PeerConnection*
PeerConnectionWrapperWithCandidateHandler::GetInternalPeerConnection() {
  auto* pci =
      static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
          pc());
  return static_cast<PeerConnection*>(pci->internal());
}

// Buffers candidates until we add them via AddBufferedIceCandidates.
void PeerConnectionObserverWithCandidateHandler::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  // If target is not set, ignore. This happens in one-ended unit tests.
  if (candidate_target_) {
    this->candidate_target_->BufferIceCandidate(candidate);
  }
  candidate_gathered_ = true;
}

}  // namespace webrtc
