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

#include <limits>
#include <utility>

#include "rtc_base/logging.h"

namespace {

// We don't pull the RTP constants from rtputils.h, to avoid a layer violation.
static const size_t kDtlsRecordHeaderLen = 13;
static const uint8_t kDtlsHandshakeType = 22;

// Extract a combination of the sequence number (6 bytes) << 2 plus
// the maximum msg_seq (2 bytes)
std::optional<uint64_t> GetDtlsHandshakeSequenceNumberAndMessageSequence(
    rtc::ArrayView<const uint8_t> dtls_packet) {
  std::optional<uint64_t> seq;
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
      seq = std::numeric_limits<uint64_t>::max();
      record_buf.Consume(len);
      continue;
    }

    // https://www.rfc-editor.org/rfc/rfc6347.html#section-4.2.2
    // TODO: a Slice() would be nice.
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
      seq = ((epoch_and_seq & 0xffffff) << 2) + msg_seq;
      // Advance outer buffer.
      record_buf.Consume(12 + fragment_len);
    }
    RTC_DCHECK(handshake_buf.Length() == 0);
  }
  // Should have consumed everything.
  if (record_buf.Length() != 0) {
    return std::nullopt;
  }
  return seq;
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

std::optional<uint64_t> DtlsStunPiggybackController::GetAckToPiggyback(
    StunMessageType stun_message_type) {
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return std::nullopt;
  }
  // Since 0 is a valid sequence number return "-1".
  if (!highest_received_dtls_sequence_number_.has_value()) {
    return std::numeric_limits<uint64_t>::max();
  }
  return *highest_received_dtls_sequence_number_;
}

void DtlsStunPiggybackController::ReportDataPiggybacked(
    const StunByteStringAttribute* data,
    const StunUInt64Attribute* ack) {
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

  // Log the acked highest received sequence number.
  if (ack != nullptr) {
    RTC_LOG(LS_VERBOSE) << "Received DTLS SEQUENCE NUMBER " << ack->value();
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

  if (data->length() == 0) {
    return;
  }

  // Extract the highest sequence number and message_seq from the packet and
  // compare it to what was been received so far.
  std::optional<uint64_t> received =
      GetDtlsHandshakeSequenceNumberAndMessageSequence(data->array_view());
  if (!received) {
    RTC_LOG(LS_ERROR) << "Failed to parse DTLS packet.";
    return;
  }
  if ((!highest_received_dtls_sequence_number_ ||
       highest_received_dtls_sequence_number_ ==
           std::numeric_limits<uint64_t>::max() ||
       received > highest_received_dtls_sequence_number_)) {
    highest_received_dtls_sequence_number_ = received;
  }

  callback_(data->array_view());
}

}  // namespace cricket
