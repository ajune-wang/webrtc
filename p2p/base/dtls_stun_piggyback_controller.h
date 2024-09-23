/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_DTLS_STUN_PIGGYBACK_CONTROLLER_H_
#define P2P_BASE_DTLS_STUN_PIGGYBACK_CONTROLLER_H_

#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/transport/stun.h"

namespace cricket {

// This class is not thread safe; all methods must be called on the same thread
// as the constructor.
class DtlsStunPiggybackController {
 public:
  DtlsStunPiggybackController(
      absl::AnyInvocable<void(rtc::ArrayView<const uint8_t>)> callback);
  ~DtlsStunPiggybackController();

  enum class State {
    // We don't know if peer support Dtls piggybacked in STUN.
    // We will piggyback dtls until we get new information or DTLS handshake
    // is complete.
    TENTATIVE = 0,
    // We are piggybacking Dtls in STUN.
    CONFIRMED = 1,
    // We are not piggybacking Dtls in STUN.
    OFF = 2,
  };

  State state() const { return state_; }  // For testing.

  // Called by DtlsTransport when handshake is complete.
  void SetDtlsHandshakeComplete();

  // Called by DtlsTransport transport when appropriate.
  void SetDataToPiggyback(rtc::ArrayView<const uint8_t> data);

  // Called by Connection, when sending a STUN BINDING { REQUEST / RESPONSE }.
  std::optional<absl::string_view> GetDataToPiggyback(
      StunMessageType stun_message_type);

  // Called by Connection when receiving a STUN BINDING { REQUEST / RESPONSE }.
  void ReportDataPiggybacked(const StunByteStringAttribute*);

 private:
  State state_ = State::TENTATIVE;
  std::string pending_packet_;
  absl::AnyInvocable<void(rtc::ArrayView<const uint8_t>)> callback_;
};

}  // namespace cricket

#endif  // P2P_BASE_DTLS_STUN_PIGGYBACK_CONTROLLER_H_
