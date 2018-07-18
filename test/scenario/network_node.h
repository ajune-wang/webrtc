/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_NETWORK_NODE_H_
#define TEST_SCENARIO_NETWORK_NODE_H_

#include <map>

#include "api/call/transport.h"
#include "call/fake_network_pipe.h"
#include "rtc_base/copyonwritebuffer.h"

namespace webrtc {
namespace test {

class NetworkReceiverInterface {
 public:
  virtual bool TrySendPacket(rtc::CopyOnWriteBuffer packet,
                             uint64_t receiver) = 0;
  virtual ~NetworkReceiverInterface() = default;
};

class NetworkNode : public NetworkReceiverInterface {
 public:
  void SetConfig(FakeNetworkPipe::Config config);
  void SetRoute(uint64_t receiver, NetworkReceiverInterface* node);
  bool TrySendPacket(rtc::CopyOnWriteBuffer packet, uint64_t receiver);
  void EnqueueCrossPacket(size_t size);

 private:
  std::map<uint64_t, NetworkReceiverInterface*> routing_table_;
};

class NetworkNodeTransport : public Transport {
 public:
  NetworkNodeTransport(NetworkNode* send_net, uint64_t receiver);
  ~NetworkNodeTransport() override;

  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* packet, size_t length) override;

 private:
  NetworkNode* const send_net_;
  const uint64_t receiver_;
};
}  // namespace test
}  // namespace webrtc
#endif  // TEST_SCENARIO_NETWORK_NODE_H_
