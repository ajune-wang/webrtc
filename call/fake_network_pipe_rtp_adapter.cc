/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/fake_network_pipe_rtp_adapter.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

void FakeNetworkPipeRtpAdapter::DeliverPacket(MediaType media_type,
                                              rtc::CopyOnWriteBuffer packet,
                                              const PacketTime& packet_time) {
  if (RtpHeaderParser::IsRtcp(packet.cdata(), packet.size())) {
    receiver_->DeliverRtcp(media_type, packet, packet_time);
    return;
  }
  RtpPacketReceived parsed_packet;
  if (parsed_packet.Parse(packet)) {
    parsed_packet.set_arrival_time_ms((packet_time.timestamp + 500) / 1000);
    // TODO(nisse): Handle RTP extensions?
    receiver_->DeliverRtp(media_type, parsed_packet);
  }
}

}  // namespace webrtc
