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

void DtlsStunPiggybackController::SetDtlsHandshakeComplete() {
  RTC_LOG(LS_ERROR) << "COMPLETE " << pending_packet_.size()
                    << " this=" << this;
  // state_ = State::OFF;
  // pending_packet_.clear();
}

void DtlsStunPiggybackController::SetDataToPiggyback(
    rtc::ArrayView<const uint8_t> data) {
  if (state_ == State::OFF) {
    RTC_LOG(LS_ERROR) << "OFF, BAIL " << data.size();
    return;
  }
  RTC_LOG(LS_ERROR) << "SET DATA " << data.size()
                    << " PREVIOUSLY=" << pending_packet_.size()
                    << " this=" << this;
  pending_packet_.assign(reinterpret_cast<const char*>(data.data()),
                         data.size());
}

std::optional<absl::string_view>
DtlsStunPiggybackController::GetDataToPiggyback(
    StunMessageType stun_message_type) {
  RTC_LOG(LS_ERROR) << "GET DATA TO PIGGYBACK " << pending_packet_.size()
                    << " STATE=" << (state_ == State::OFF);
  if (state_ == State::OFF) {
    return std::nullopt;
  }

  switch (stun_message_type) {
    case STUN_BINDING_REQUEST:
      if (pending_packet_.size() == 0) {
        return std::nullopt;
      }
      break;
    case STUN_BINDING_RESPONSE:
      if (state_ == State::TENTATIVE) {
        RTC_LOG(LS_ERROR) << "NOT HERE " << pending_packet_.size();
        return std::nullopt;
      }
      break;
    default:
      return std::nullopt;
  }
  return absl::string_view(pending_packet_);
}

void DtlsStunPiggybackController::ReportDataPiggybacked(
    const StunByteStringAttribute* data) {
  if (state_ == State::OFF) {
    return;
  }

  // We sent dtls piggybacked but got nothing in return => peer does not
  // support.
  if (data == nullptr) {
    state_ = State::OFF;
    pending_packet_.clear();
    RTC_LOG(LS_ERROR) << "TURN OFF";
    return;
  }

  // We sent dtls piggybacked and got something in return => peer does support.
  if (state_ == State::TENTATIVE) {
    state_ = State::CONFIRMED;
  }

  if (data->length() == 0) {
    return;
  }

  callback_(data->array_view());
}

}  // namespace cricket
