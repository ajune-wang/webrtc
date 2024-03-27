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
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/types/optional.h"
#include "modules/video_coding/h264_sps_pps_tracker.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {
namespace video_coding {

class H26xPacketBuffer {
 public:
  // The H26xPacketBuffer does the same job as the PacketBuffer but for H264 and
  // H265 only. To make it fit in with surronding code the PacketBuffer
  // input/output classes are used.
  using Packet = video_coding::PacketBuffer::Packet;
  using InsertResult = video_coding::PacketBuffer::InsertResult;

  // If |tracker| is nullptr, H.264 IDR frames without SPS and PPS are not
  // allowed. H.265 always requires in band paramter sets.
  explicit H26xPacketBuffer(
      std::unique_ptr<video_coding::H264SpsPpsTracker> tracker);

  ABSL_MUST_USE_RESULT InsertResult
  InsertPacket(std::unique_ptr<Packet> packet);

  // Out of band supplied codec parameters for H.264.
  void SetSpropParameterSets(const std::string& sprop_parameter_sets);

 private:
  static constexpr int kBufferSize = 2048;

  std::unique_ptr<Packet>& GetPacket(int64_t unwrapped_seq_num);
  bool BeginningOfStream(const Packet& packet) const;
  InsertResult FindFrames(int64_t unwrapped_seq_num);
  bool MaybeAssembleFrame(int64_t start_seq_num_unwrapped,
                          int64_t end_sequence_number_unwrapped,
                          InsertResult& result);

  std::array<std::unique_ptr<Packet>, kBufferSize> buffer_;
  absl::optional<int64_t> last_continuous_unwrapped_seq_num_;
  SeqNumUnwrapper<uint16_t> seq_num_unwrapper_;
  std::unique_ptr<video_coding::H264SpsPpsTracker> tracker_;
  // Indicates whether sprop parameter sets are provided. If H.264 IDR frames
  // without SPS and PPS are not allowed, this value should always be false.
  bool h264_out_of_band_sps_pps_ = false;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_H26X_PACKET_BUFFER_H_
