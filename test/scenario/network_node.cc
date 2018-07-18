/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/network_node.h"

namespace webrtc {
namespace test {

void NetworkNode::SetConfig(FakeNetworkPipe::Config config) {}

void NetworkNode::SetRoute(uint64_t receiver, NetworkReceiverInterface* node) {
  routing_table_[receiver] = node;
}

bool NetworkNode::TrySendPacket(rtc::CopyOnWriteBuffer packet,
                                uint64_t receiver) {
  return false;
}

void NetworkNode::EnqueueCrossPacket(size_t size) {}

NetworkNodeTransport::NetworkNodeTransport(NetworkNode* send_net,
                                           uint64_t receiver)
    : send_net_(send_net), receiver_(receiver) {}

NetworkNodeTransport::~NetworkNodeTransport() = default;
bool NetworkNodeTransport::SendRtp(const uint8_t* packet,
                                   size_t length,
                                   const PacketOptions& options) {
  return send_net_->TrySendPacket(rtc::CopyOnWriteBuffer(packet, length),
                                  receiver_);
}

bool NetworkNodeTransport::SendRtcp(const uint8_t* packet, size_t length) {
  return send_net_->TrySendPacket(rtc::CopyOnWriteBuffer(packet, length),
                                  receiver_);
}

}  // namespace test
}  // namespace webrtc
