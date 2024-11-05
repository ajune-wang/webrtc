/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/dtls_stun_piggyback_controller.h"

#include <utility>

#include "rtc_base/logging.h"

namespace cricket {

DtlsStunPiggybackController::DtlsStunPiggybackController(
    absl::AnyInvocable<void(rtc::ArrayView<const uint8_t>)> callback)
    : callback_(std::move(callback)) {}

DtlsStunPiggybackController::~DtlsStunPiggybackController() {}

void DtlsStunPiggybackController::SetDtlsHandshakeComplete(
    bool is_dtls_client) {
  // Peer does not support this so fallback to a normal DTLS handshake
  // happened.
  if (state_ == State::OFF) {
    return;
  }
  // As DTLS server we need to keep the last flight around until
  // we receive the post-handshake acknowledgment.
  // As DTLS client we have nothing more to send at this point
  // but will continue to send ACK attributes until receiving
  // the last flight from the server.
  state_ = State::PENDING;
  if (is_dtls_client) {
    pending_packet_.clear();
  }
}

void DtlsStunPiggybackController::SetDataToPiggyback(
    rtc::ArrayView<const uint8_t> data) {
  if (state_ == State::OFF) {
    // DCHECK?
    return;
  }
  // TODO: do we are about overwriting? We might clear when the
  // data is acked...
  pending_packet_.assign(reinterpret_cast<const char*>(data.data()),
                         data.size());
}

std::optional<absl::string_view>
DtlsStunPiggybackController::GetDataToPiggyback(
    StunMessageType stun_message_type) {
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return std::nullopt;
  }
  if (pending_packet_.size() == 0) {
    return std::nullopt;
  }

  switch (stun_message_type) {
    case STUN_BINDING_REQUEST:
    case STUN_BINDING_RESPONSE:
      break;
    default:
      // TODO: notreached?
      return std::nullopt;
  }
  return absl::string_view(pending_packet_);
}

std::optional<uint32_t> DtlsStunPiggybackController::GetAckToPiggyback(
    StunMessageType stun_message_type) {
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return std::nullopt;
  }
  // TODO: actually do something meaningful.
  return 0;
}

void DtlsStunPiggybackController::ReportDataPiggybacked(
    const StunByteStringAttribute* data,
    const StunUInt32Attribute* ack) {
  RTC_LOG(LS_ERROR) << "DATA=" << !!data << " ACK=" << !!ack;
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return;
  }
  RTC_LOG(LS_VERBOSE) << "State " << static_cast<int>(state_)
                      << " data set: " << !!data << " ack set: " << !!ack;

  // We sent dtls piggybacked but got nothing in return or
  // we received a stun request with neither attribute set
  // => peer does not support.
  if (state_ == State::TENTATIVE && data == nullptr && ack == nullptr) {
    state_ = State::OFF;
    pending_packet_.clear();
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking not supported by peer.";
    return;
  }

  // In PENDING state the peer may have stopped sending the ack
  // when it moved to the COMPLETE state. Move to the same state.
  if (state_ == State::PENDING && data == nullptr && ack == nullptr) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking completed.";
    state_ = State::COMPLETE;
    pending_packet_.clear();
    return;
  }

  // We sent dtls piggybacked and got something in return => peer does support.
  if (state_ == State::TENTATIVE) {
    state_ = State::CONFIRMED;
  }

  if (ack != nullptr) {
    // TODO: Log the DTLS sequence number.
  }
  // The response to the final flight of the handshake will not contain
  // the DTLS data but will contain an ack.
  // Must not happen on the initial server to client packet which
  // has no DTLS data yet.
  if (data == nullptr && ack != nullptr && state_ == State::PENDING) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking completed.";
    state_ = State::COMPLETE;
    pending_packet_.clear();
    return;
  }
  if (!data) {
    return;
  }

  // TODO: Extract the DTLS sequence number and store it.

  if (data->length() == 0) {
    return;
  }

  callback_(data->array_view());
}

}  // namespace cricket
