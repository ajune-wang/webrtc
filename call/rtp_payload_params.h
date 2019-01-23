/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_PAYLOAD_PARAMS_H_
#define CALL_RTP_PAYLOAD_PARAMS_H_

#include <map>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "call/rtp_config.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

class RTPFragmentationHeader;
class RtpRtcp;

struct CodecSpecificInfoVP8;

// State for setting picture id and tl0 pic idx, for VP8 and VP9
// TODO(nisse): Make these properties not codec specific.
class RtpPayloadParams final {
 public:
  RtpPayloadParams(const uint32_t ssrc, const RtpPayloadState* state);
  RtpPayloadParams(const RtpPayloadParams& other);
  ~RtpPayloadParams();

  RTPVideoHeader GetRtpVideoHeader(const EncodedImage& image,
                                   const CodecSpecificInfo* codec_specific_info,
                                   int64_t shared_frame_id);

  uint32_t ssrc() const;

  RtpPayloadState state() const;

 private:
  void SetCodecSpecific(RTPVideoHeader* rtp_video_header,
                        bool first_frame_in_picture);
  void SetGeneric(const CodecSpecificInfo* codec_specific_info,
                  int64_t frame_id,
                  bool is_keyframe,
                  RTPVideoHeader* rtp_video_header);

  void Vp8ToGeneric(const CodecSpecificInfoVP8& vp8_info,
                    int64_t shared_frame_id,
                    bool is_keyframe,
                    RTPVideoHeader* rtp_video_header);

  // TODO: !!!
  void Vp8ManageExplicitDependencies(
      const CodecSpecificInfoVP8& vp8_info,
      int64_t shared_frame_id,
      bool is_keyframe,
      RTPVideoHeader::GenericDescriptorInfo* generic);

  // Holds the last shared frame id for a given (spatial, temporal) layer.
  std::array<std::array<int64_t, RtpGenericFrameDescriptor::kMaxTemporalLayers>,
             RtpGenericFrameDescriptor::kMaxSpatialLayers>
      last_shared_frame_id_;

  // TODO(eladalon): 1. Remove |last_shared_frame_id_|. 2. If we use p_diffs,
  // remove |vp8_shared_frame_id_by_buffer_| as well.
  std::array<int64_t, 3> vp8_shared_frame_id_by_buffer_;

  const uint32_t ssrc_;
  RtpPayloadState state_;

  const bool generic_picture_id_experiment_;
  const bool generic_descriptor_experiment_;
};
}  // namespace webrtc
#endif  // CALL_RTP_PAYLOAD_PARAMS_H_
