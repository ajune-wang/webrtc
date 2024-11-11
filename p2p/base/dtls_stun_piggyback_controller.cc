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
#include <vector>

#include "rtc_base/logging.h"

namespace {

// We don't pull the RTP constants from rtputils.h, to avoid a layer violation.
static const size_t kDtlsRecordHeaderLen = 13;
static const uint8_t kDtlsHandshakeType = 22;

// Returns a (unsorted) list of (msg_seq) received as part of the handshake.
std::optional<std::vector<uint16_t>> GetDtlsHandshakeAcks(
    rtc::ArrayView<const uint8_t> dtls_packet) {
  std::vector<uint16_t> acks;
  rtc::ByteBufferReader record_buf(dtls_packet);
  // https://datatracker.ietf.org/doc/html/rfc6347#section-4.1
  while (record_buf.Length() >= kDtlsRecordHeaderLen) {
    uint8_t content_type;
    uint64_t epoch_and_seq;
    uint16_t len;
    // Read content_type(1), skip version(2), read epoch+seq(2+6),
    // read len(2)
    if (!(record_buf.ReadUInt8(&content_type) && record_buf.Consume(2) &&
          record_buf.ReadUInt64(&epoch_and_seq) &&
          record_buf.ReadUInt16(&len) && record_buf.Length() >= len)) {
      return std::nullopt;
    }
    if (content_type != kDtlsHandshakeType) {
      record_buf.Consume(len);
      continue;
    }
    // Epoch 1+ is encrypted so we can not parse it.
    if (epoch_and_seq >> 6 != 0) {
      record_buf.Consume(len);
      continue;
    }

    // https://www.rfc-editor.org/rfc/rfc6347.html#section-4.2.2
    rtc::ByteBufferReader handshake_buf(record_buf.DataView().subview(0, len));
    while (handshake_buf.Length() > 0) {
      uint16_t msg_seq;
      uint32_t fragment_len;
      uint32_t fragment_offset;
      // Skip msg_type(1) and length(3), read msg_seq(2), skip
      // fragment_offset(3), read fragment_length(3) and consume it.
      if (!(handshake_buf.Consume(1 + 3) &&
            handshake_buf.ReadUInt16(&msg_seq) &&
            handshake_buf.ReadUInt24(&fragment_offset) &&
            handshake_buf.ReadUInt24(&fragment_len) &&
            handshake_buf.Consume(fragment_len))) {
        return std::nullopt;
      }
      acks.push_back(msg_seq);
      // Advance outer buffer.
      record_buf.Consume(12 + fragment_len);
    }
    RTC_DCHECK(handshake_buf.Length() == 0);
  }
  // Should have consumed everything.
  if (record_buf.Length() != 0) {
    return std::nullopt;
  }
  return acks;
}

}  // namespace

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
    // TODO: DCHECK?
    return;
  }
  // TODO: do we care about overwriting? We might clear when the data is
  // acked...
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

std::optional<absl::string_view> DtlsStunPiggybackController::GetAckToPiggyback(
    StunMessageType stun_message_type) {
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return std::nullopt;
  }
  return handshake_ack_writer_.DataAsStringView();
}

void DtlsStunPiggybackController::ReportDataPiggybacked(
    const StunByteStringAttribute* data,
    const StunByteStringAttribute* ack) {
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
    handshake_messages_received_.clear();
    return;
  }

  // We sent dtls piggybacked and got something in return => peer does support.
  if (state_ == State::TENTATIVE) {
    state_ = State::CONFIRMED;
  }

  // Log the acked highest received sequence number.
  if (ack != nullptr) {
    RTC_LOG(LS_VERBOSE) << "Received DTLS ACK";  // TODO
  }
  // The response to the final flight of the handshake will not contain
  // the DTLS data but will contain an ack.
  // Must not happen on the initial server to client packet which
  // has no DTLS data yet.
  if (data == nullptr && ack != nullptr && state_ == State::PENDING) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking completed.";
    state_ = State::COMPLETE;
    pending_packet_.clear();
    handshake_messages_received_.clear();
    return;
  }
  if (!data) {
    return;
  }

  if (data->length() == 0) {
    return;
  }

  // Extract the received message sequence numbers of the handshake
  // from the packet and prepare the ack to be sent.
  std::optional<std::vector<uint16_t>> new_message_sequences =
      GetDtlsHandshakeAcks(data->array_view());
  if (!new_message_sequences) {
    RTC_LOG(LS_ERROR) << "Failed to parse DTLS packet.";
    return;
  }
  if (!new_message_sequences->empty()) {
    for (const auto& message_seq : *new_message_sequences) {
      handshake_messages_received_.insert(message_seq);
    }
    handshake_ack_writer_.Clear();
    for (const auto& message_seq : handshake_messages_received_) {
      handshake_ack_writer_.WriteUInt16(message_seq);
    }
  }

  callback_(data->array_view());
}

}  // namespace cricket
