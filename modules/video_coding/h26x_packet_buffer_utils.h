/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_H26X_PACKET_BUFFER_UTILS_H_
#define MODULES_VIDEO_CODING_H26X_PACKET_BUFFER_UTILS_H_

#include <memory>
#include <set>
#include <vector>

#include "modules/video_coding/packet_buffer.h"

namespace webrtc {
namespace video_coding {

// Process H.264 specific logic.
void ProcessH264Packets(
    uint16_t seq_num,
    uint16_t start,
    size_t index,
    bool idr_only_keyframes_allowed,
    std::vector<std::unique_ptr<PacketBuffer::Packet>>& buffer,
    std::vector<std::unique_ptr<PacketBuffer::Packet>>& found_frames,
    std::set<uint16_t, DescendingSeqNumComp<uint16_t>>& missing_packets,
    std::set<uint16_t, DescendingSeqNumComp<uint16_t>>& received_padding);

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_H26X_PACKET_BUFFER_UTILS_H_
