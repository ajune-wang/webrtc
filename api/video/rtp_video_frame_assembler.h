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
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_av1.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_generic.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_raw.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp8.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp9.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"

namespace webrtc {
class RtpVideoFrameAssembler {
 public:
  using ReturnVector = absl::InlinedVector<std::unique_ptr<RtpFrameObject>, 3>;
  enum PayloadFormat { kRaw, kH264, kVp8, kVp9, kAv1, kGeneric };

  explicit RtpVideoFrameAssembler(PayloadFormat payload_format);

  ReturnVector InsertPacket(const RtpPacketReceived& packet);
  void ClearTo(int64_t frame_id);

 private:
  ReturnVector AssembleFrames(
      video_coding::PacketBuffer::InsertResult insert_result);
  ReturnVector FindReferences(ReturnVector frames);
  ReturnVector UpdateWithPadding(uint16_t seq_num);
  void SaveFrameIdToSeqNumMapping(const ReturnVector& frames);
  bool ParseDependenciesDescriptorExtension(const RtpPacketReceived& rtp_packet,
                                            RTPVideoHeader& video_header);
  bool ParseGenericDescriptorExtension(const RtpPacketReceived& rtp_packet,
                                       RTPVideoHeader& video_header);
  VideoRtpDepacketizer& GetDepacketizer();

  std::unique_ptr<FrameDependencyStructure> video_structure_;
  SeqNumUnwrapper<uint16_t> frame_id_unwrapper_;
  absl::optional<int64_t> video_structure_frame_id_;

  absl::variant<VideoRtpDepacketizerRaw,
                VideoRtpDepacketizerH264,
                VideoRtpDepacketizerVp8,
                VideoRtpDepacketizerVp9,
                VideoRtpDepacketizerAv1,
                VideoRtpDepacketizerGeneric>
      depacketizer_;
  video_coding::PacketBuffer packet_buffer_;

  RtpFrameReferenceFinder reference_finder_;

  std::map<int64_t, uint16_t> frame_id_to_seq_num_;
};

}  // namespace webrtc

#endif  // API_VIDEO_RTP_VIDEO_FRAME_ASSEMBLER_H_
