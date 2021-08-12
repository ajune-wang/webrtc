/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/socket/packet_sender.h"

#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "net/dcsctp/public/types.h"

namespace dcsctp {

PacketSender::PacketSender(TimerManager& timer_manager,
                           DcSctpSocketCallbacks& callbacks,
                           std::function<void(rtc::ArrayView<const uint8_t>,
                                              SendPacketStatus)> on_sent_packet)
    : callbacks_(callbacks),
      on_sent_packet_(std::move(on_sent_packet)),
      retry_timer_(timer_manager.CreateTimer(
          "packet-retry",
          absl::bind_front(&PacketSender::OnRetryTimerExpiry, this),
          TimerOptions(DurationMs(1)))) {}

absl::optional<DurationMs> PacketSender::OnRetryTimerExpiry() {
  RetrySendPackets();
  return absl::nullopt;
}

bool PacketSender::RetrySendPackets() {
  if (retry_queue_.empty()) {
    RTC_DCHECK(!retry_timer_->is_running());
    return true;
  }

  while (!retry_queue_.empty()) {
    SendPacketStatus status =
        callbacks_.SendPacketWithStatus(retry_queue_.front());
    on_sent_packet_(retry_queue_.front(), status);
    switch (status) {
      case SendPacketStatus::kSuccess:
        retry_queue_.pop_front();
        continue;
      case SendPacketStatus::kTemporaryFailure:
        return false;
      case SendPacketStatus::kError:
        retry_queue_.pop_front();
        return false;
    }
  }

  RTC_DCHECK(retry_timer_->is_running());
  retry_timer_->Stop();
  return true;
}

bool PacketSender::Send(SctpPacket::Builder& builder) {
  if (builder.empty()) {
    return false;
  }

  std::vector<uint8_t> payload = builder.Build();

  SendPacketStatus status = callbacks_.SendPacketWithStatus(payload);
  on_sent_packet_(payload, status);
  switch (status) {
    case SendPacketStatus::kSuccess: {
      return true;
    }
    case SendPacketStatus::kTemporaryFailure: {
      retry_queue_.emplace_back(std::move(payload));
      if (!retry_timer_->is_running()) {
        retry_timer_->Start();
      }
      return false;
    }

    case SendPacketStatus::kError: {
      // Nothing that can be done.
      return false;
    }
  }
}
}  // namespace dcsctp
