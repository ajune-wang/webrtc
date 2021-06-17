/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_RTP_VIDEO_FRAME_ASSEMBLER_H_
#define API_VIDEO_RTP_VIDEO_FRAME_ASSEMBLER_H_

#include <map>
#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/variant.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/video_coding/frame_object.h"

namespace webrtc {
namespace internal {
class RtpVideoFrameAssemblerImpl;
}  // namespace internal

// The RtpVideoFrameAssembler takes RtpPacketReceived and assembles them into
// complete frames. A frame is considered complete when all packets of the frame
// has been received, the bitstream data has successfully extracted, an ID has
// been assigned, and all dependencies are know. Frame IDs are strictly
// monotonic in decode order, dependencies are expressed as frame IDs.
class RtpVideoFrameAssembler {
 public:
  // Convenience alias for long return type.
  using ReturnVector = absl::InlinedVector<std::unique_ptr<RtpFrameObject>, 3>;

  enum PayloadFormat { kRaw, kH264, kVp8, kVp9, kAv1, kGeneric };

  explicit RtpVideoFrameAssembler(PayloadFormat payload_format);
  RtpVideoFrameAssembler(const RtpVideoFrameAssembler& other) = delete;
  RtpVideoFrameAssembler& operator=(const RtpVideoFrameAssembler& other) =
      delete;
  ~RtpVideoFrameAssembler();

  ReturnVector InsertPacket(const RtpPacketReceived& packet);

  // When the receiver is no longer interested in frames past a certain point
  // (typically after the decoding of a frame) then `ClearTo` should be called
  // to discard incomplete frames that are prior to the frame with `frame_id`
  // in decode order. It is not critical that `ClearTo` is called immediately
  // but it should be called regularly to avoid old packets conflict with new
  // packets after an RTP packet sequence number wraparound.
  void ClearTo(int64_t frame_id);

 private:
  std::unique_ptr<internal::RtpVideoFrameAssemblerImpl> impl_;
};

}  // namespace webrtc

#endif  // API_VIDEO_RTP_VIDEO_FRAME_ASSEMBLER_H_
