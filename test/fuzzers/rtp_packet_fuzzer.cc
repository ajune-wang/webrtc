/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <bitset>
#include <vector>

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_video_layers_allocation_extension.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size <= 4)
    return;

  // We decide which header extensions to register by reading four bytes
  // from the beginning of `data` and interpreting it as a bitmask over
  // the RTPExtensionType enum. This assert ensures four bytes are enough.
  auto ext = RtpHeaderExtensionMap::KnownExtensions();
  RTC_CHECK_LE(ext.size(), 32)
      << "Insufficient bits read to configure all header extensions. Add "
         "an extra byte and update the switches.";

  // Don't use the configuration byte as part of the packet.
  std::bitset<32> extension_mask(*reinterpret_cast<const uint32_t*>(data));
  data += 4;
  size -= 4;

  RtpPacketReceived::ExtensionManager extensions(/*extmap_allow_mixed=*/true);
  // Start at local_id = 1 since 0 is an invalid extension id.
  int local_id = 1;
  for (size_t i = 0; i < ext.size(); i++) {
    if (extension_mask[i]) {
      // Extensions are registered with an ID, which you signal to the
      // peer so they know what to expect. This code only cares about
      // parsing so the value of the ID isn't relevant.
      extensions.RegisterByUri(local_id++, ext[i]);
    }
  }

  RtpPacketReceived packet(&extensions);
  packet.Parse(data, size);

  // Call packet accessors because they have extra checks.
  packet.Marker();
  packet.PayloadType();
  packet.SequenceNumber();
  packet.Timestamp();
  packet.Ssrc();
  packet.Csrcs();

  // Each extension has its own getter. It is supported behaviour to
  // call GetExtension on an extension which was not registered, so we
  // don't check the bitmask here.
  packet.GetExtension<TransmissionOffset>();
  {
    bool voice_activity;
    uint8_t audio_level;
    packet.GetExtension<AudioLevel>(&voice_activity, &audio_level);
  }
  {
    std::vector<uint8_t> csrc_audio_levels;
    packet.GetExtension<CsrcAudioLevel>(&csrc_audio_levels);
  }
  packet.GetExtension<AbsoluteSendTime>();
  packet.GetExtension<AbsoluteCaptureTimeExtension>();
  packet.GetExtension<VideoOrientation>();
  packet.GetExtension<TransportSequenceNumber>();
  {
    uint16_t seqnum;
    absl::optional<FeedbackRequest> feedback_request;
    packet.GetExtension<TransportSequenceNumberV2>(&seqnum, &feedback_request);
  }
  packet.GetExtension<PlayoutDelayLimits>();
  packet.GetExtension<VideoContentTypeExtension>();
  packet.GetExtension<VideoTimingExtension>();
  packet.GetExtension<RtpStreamId>();
  packet.GetExtension<RepairedRtpStreamId>();
  packet.GetExtension<RtpMid>();
  packet.GetExtension<RtpGenericFrameDescriptorExtension00>();
  packet.GetExtension<ColorSpaceExtension>();
  packet.GetExtension<InbandComfortNoiseExtension>();
  packet.GetExtension<RtpVideoLayersAllocationExtension>();
  packet.GetExtension<VideoFrameTrackingIdExtension>();

  // Check that zero-ing mutable extensions wouldn't cause any problems.
  packet.ZeroMutableExtensions();
}
}  // namespace webrtc
