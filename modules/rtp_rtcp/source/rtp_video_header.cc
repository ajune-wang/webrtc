/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

RTPVideoHeader::RTPVideoHeader() : video_timing() {}
RTPVideoHeader::RTPVideoHeader(const RTPVideoHeader& other) = default;
RTPVideoHeader::~RTPVideoHeader() = default;

RTPVideoHeader::GenericDescriptorInfo::GenericDescriptorInfo() = default;
RTPVideoHeader::GenericDescriptorInfo::GenericDescriptorInfo(
    const GenericDescriptorInfo& other) = default;
RTPVideoHeader::GenericDescriptorInfo::~GenericDescriptorInfo() = default;

// Creates raw representation of the frame dependencies for authentication
std::vector<uint8_t> AuthenticationBytes(
    const RTPVideoHeader::GenericDescriptorInfo& descriptor) {
  RtpGenericFrameDescriptor frame_descriptor;
  frame_descriptor.SetFirstPacketInSubFrame(true);
  frame_descriptor.SetLastPacketInSubFrame(false);
  frame_descriptor.SetTemporalLayer(descriptor.temporal_index);
  frame_descriptor.SetSpatialLayersBitmask(1 << descriptor.spatial_index);
  frame_descriptor.SetFrameId(descriptor.frame_id & 0xFFFF);
  for (int64_t dependency : descriptor.dependencies) {
    frame_descriptor.AddFrameDependencyDiff(descriptor.frame_id - dependency);
  }
  std::vector<uint8_t> result(
      RtpGenericFrameDescriptorExtension00::ValueSize(frame_descriptor));
  RtpGenericFrameDescriptorExtension00::Write(result, frame_descriptor);
  return result;
}

}  // namespace webrtc
