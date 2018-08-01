/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_FAKE_NETWORK_PIPE_RTP_ADAPTER_H_
#define CALL_FAKE_NETWORK_PIPE_RTP_ADAPTER_H_

#include "call/call.h"
#include "call/fake_network_pipe.h"

namespace webrtc {

// Registered as a receiver with FakeNetworkPipe, to parse RTP packets and
// forward to Call's PacketReceiver.
class FakeNetworkPipeRtpAdapter : public RawPacketReceiver {
 public:
  explicit FakeNetworkPipeRtpAdapter(PacketReceiver* receiver)
      : receiver_(receiver) {}
  void DeliverPacket(MediaType media_type,
                     rtc::CopyOnWriteBuffer packet,
                     const PacketTime& packet_time) override;

 private:
  PacketReceiver* receiver_;
};

}  // namespace webrtc

#endif  // CALL_FAKE_NETWORK_PIPE_RTP_ADAPTER_H_
