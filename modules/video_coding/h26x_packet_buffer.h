/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_H26X_PACKET_BUFFER_H_
#define MODULES_VIDEO_CODING_H26X_PACKET_BUFFER_H_

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {

class H26xPacketBuffer {
 public:
  // The H26xPacketBuffer does the same job as the PacketBuffer but for H264 and
  // H265 only. To make it fit in with surronding code the PacketBuffer
  // input/output classes are used.
  using Packet = video_coding::PacketBuffer::Packet;
  using InsertResult = video_coding::PacketBuffer::InsertResult;

  // |h264_idr_only_keyframes_allowed| is ignored if H.265 is used.
  explicit H26xPacketBuffer(bool h264_idr_only_keyframes_allowed);

  ABSL_MUST_USE_RESULT InsertResult
  InsertPacket(std::unique_ptr<Packet> packet);

 private:
  static constexpr int kBufferSize = 2048;
  static constexpr int kNumTrackedSequences = 5;

  std::unique_ptr<Packet>& GetPacket(int64_t unwrapped_seq_num);
  bool BeginningOfStream(const Packet& packet) const;
  InsertResult FindFrames(int64_t unwrapped_seq_num);
  bool MaybeAssembleFrame(int64_t start_seq_num_unwrapped,
                          int64_t end_sequence_number_unwrapped,
                          InsertResult& result);

  // Indicates whether IDR frames without SPS and PPS are allowed.
  const bool h264_idr_only_keyframes_allowed_;
  std::array<std::unique_ptr<Packet>, kBufferSize> buffer_;
  std::array<int64_t, kNumTrackedSequences> last_continuous_in_sequence_;
  int64_t last_continuous_in_sequence_index_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_H26X_PACKET_BUFFER_H_
