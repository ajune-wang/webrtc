/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/unhandled_packets_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace cricket {

UnhandledPacketsBuffer::UnhandledPacketsBuffer(bool enabled) {
  if (enabled) {
    buffer_.reserve(kMaxStashedPackets);
  }
}

UnhandledPacketsBuffer::~UnhandledPacketsBuffer() {}

// Store packet in buffer.
void UnhandledPacketsBuffer::AddPacket(uint32_t ssrc,
                                       rtc::CopyOnWriteBuffer packet) {
  if (buffer_.size() < kMaxStashedPackets) {
    buffer_.emplace_back(std::make_pair(ssrc, packet));
  } else {
    buffer_[insert_pos_] = std::make_pair(ssrc, packet);
  }
  insert_pos_ = (insert_pos_ + 1) % kMaxStashedPackets;
}

// Backfill |receiver| with all stored packet related |ssrcs|.
void UnhandledPacketsBuffer::BackfillPackets(std::vector<uint32_t> ssrcs,
                                             webrtc::PacketReceiver* receiver) {
  size_t start, end;
  if (buffer_.size() < kMaxStashedPackets) {
    start = 0;
    end = buffer_.size();
  } else {
    start = insert_pos_;
    end = insert_pos_ == 0 ? buffer_.size() : insert_pos_ - 1;
  }

  size_t count = 0;
  for (size_t pos = start; pos != end; pos++) {
    // One or maybe 2 ssrcs is expected. loop array instead of more elaborate
    // scheme.
    uint32_t ssrc = buffer_[pos].first;
    if (std::find(ssrcs.begin(), ssrcs.end(), ssrc) != ssrcs.end()) {
      count++;
      receiver->DeliverPacket(webrtc::MediaType::VIDEO, buffer_[pos].second,
                              -1);
    }
  }

  rtc::StringBuilder out;
  out << "[ ";
  for (const auto& ssrc : ssrcs) {
    out << std::to_string(ssrc) << " ";
  }
  out << "]";
  RTC_LOG(LS_INFO) << "Backfilled " << count
                   << " packets for ssrcs: " << out.Release();
}

}  // namespace cricket
