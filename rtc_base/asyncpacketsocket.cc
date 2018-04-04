/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/asyncpacketsocket.h"

namespace rtc {

PacketTimeUpdateParams::PacketTimeUpdateParams()
    : rtp_sendtime_extension_id(-1),
      srtp_auth_tag_len(-1),
      srtp_packet_index(-1) {
}

PacketTimeUpdateParams::PacketTimeUpdateParams(
    const PacketTimeUpdateParams& other) = default;

PacketTimeUpdateParams::~PacketTimeUpdateParams() = default;

PacketOptions::PacketOptions() : dscp(DSCP_NO_CHANGE), packet_id(-1) {}
PacketOptions::PacketOptions(DiffServCodePoint dscp)
    : dscp(dscp), packet_id(-1) {}
PacketOptions::PacketOptions(const PacketOptions& other) = default;
PacketOptions::~PacketOptions() = default;

AsyncPacketSocket::AsyncPacketSocket() {
}

AsyncPacketSocket::~AsyncPacketSocket() {
}

rtc::PacketInfo GenerateSentPacketInfo(const PacketOptions& options,
                                       size_t packet_size_bytes,
                                       const AsyncPacketSocket& socket_from) {
  PacketInfo info;
  info.packet_type = options.packet_type;
  info.protocol = options.protocol;
  info.port_type = options.port_type;
  info.network = options.network;
  info.packet_size_bytes = packet_size_bytes;
  info.local_socket_address = socket_from.GetLocalAddress();
  info.remote_socket_address = socket_from.GetRemoteAddress();
  return info;
}

};  // namespace rtc
