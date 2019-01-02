/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_NETWORK_H_
#define TEST_SCENARIO_NETWORK_NETWORK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "rtc_base/asyncsocket.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread.h"

namespace webrtc {

struct EmulatedIpPacket {
 public:
  EmulatedIpPacket(const rtc::SocketAddress& from,
                   const rtc::SocketAddress to,
                   std::string dest_endpoint_id,
                   rtc::CopyOnWriteBuffer data,
                   Timestamp sent_time);
  ~EmulatedIpPacket();

  rtc::SocketAddress from;
  rtc::SocketAddress to;
  std::string dest_endpoint_id;
  rtc::CopyOnWriteBuffer data;
  Timestamp sent_time;
};

class NetworkReceiverInterface {
 public:
  explicit NetworkReceiverInterface(std::string id) : id_(id) {}
  virtual ~NetworkReceiverInterface() = default;

  // Should be used only for logging. No unique guarantees provided.
  std::string GetId() const { return id_; }
  void DeliverPacket(std::unique_ptr<EmulatedIpPacket> packet) {
    DeliverPacketInternal(std::move(packet));
  }

 protected:
  virtual void DeliverPacketInternal(
      std::unique_ptr<EmulatedIpPacket> packet) = 0;

 private:
  // Node id for logging purpose.
  const std::string id_;
};

}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_NETWORK_H_
