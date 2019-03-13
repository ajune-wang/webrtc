/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_UNHANDLED_PACKETS_BUFFER_H_
#define MEDIA_ENGINE_UNHANDLED_PACKETS_BUFFER_H_

#include <stdint.h>
#include <utility>
#include <vector>

#include "call/packet_receiver.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace cricket {

class UnhandledPacketsBuffer {
 public:
  UnhandledPacketsBuffer();
  ~UnhandledPacketsBuffer();

  // Store packet in buffer.
  void AddPacket(uint32_t ssrc, rtc::CopyOnWriteBuffer packet);

  // Backfill |receiver| with all stored packet related |ssrcs|.
  void BackfillPackets(std::vector<uint32_t> ssrcs,
                       webrtc::PacketReceiver* reciver);

  const size_t kMaxStashedPackets = 50;

 private:
  size_t insert_pos_ = 0;
  std::vector<std::pair<uint32_t, rtc::CopyOnWriteBuffer>> buffer_;
};

}  // namespace cricket

#endif  // MEDIA_ENGINE_UNHANDLED_PACKETS_BUFFER_H_
