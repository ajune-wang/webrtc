/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_
#define P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_

#include <cstdint>
#include <set>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/transport/stun.h"

namespace cricket {

// This class is not thread safe; all methods must be called on the same thread
// as the constructor.
class DtlsStunPiggybackController {
 public:
  // dtls_data_callback will be called with any DTLS packets received
  // piggybacked.
  DtlsStunPiggybackController(
      absl::AnyInvocable<void(rtc::ArrayView<const uint8_t>)>
          dtls_data_callback);
  ~DtlsStunPiggybackController();

  enum class State {
    // We don't know if peer support DTLS piggybacked in STUN.
    // We will piggyback DTLS until we get new information or
    // the DTLS handshake is complete.
    TENTATIVE = 0,
    // The peer supports DTLS in STUN and we continue the handshake.
    CONFIRMED = 1,
    // We are waiting for the final ack. Semantic differs depending
    // on DTLS role.
    PENDING = 2,
    // We successfully completed the DTLS handshake in STUN.
    COMPLETE = 3,
    // The peer does not support piggybacking DTLS in STUN.
    OFF = 4,
  };

  State state() const { return state_; }

  // Called by DtlsTransport when handshake is complete.
  void SetDtlsHandshakeComplete(bool is_dtls_client);

  // Called by DtlsTransport transport when there is data to piggyback.
  void SetDataToPiggyback(rtc::ArrayView<const uint8_t> data);

  // Called by Connection, when sending a STUN BINDING { REQUEST / RESPONSE }
  // to obtain optional DTLS data or ACKs.
  std::optional<absl::string_view> GetDataToPiggyback(
      StunMessageType stun_message_type);
  std::optional<absl::string_view> GetAckToPiggyback(
      StunMessageType stun_message_type);

  // Called by Connection when receiving a STUN BINDING { REQUEST / RESPONSE }.
  void ReportDataPiggybacked(const StunByteStringAttribute* data,
                             const StunByteStringAttribute* ack);

 private:
  State state_ = State::TENTATIVE;
  std::string pending_packet_;
  absl::AnyInvocable<void(rtc::ArrayView<const uint8_t>)> dtls_data_callback_;

  std::set<uint16_t> handshake_messages_received_;
  rtc::ByteBufferWriter handshake_ack_writer_;
};

}  // namespace cricket

#endif  // P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_
