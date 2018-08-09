/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_BASIC_MDNS_RESPONDER_H_
#define P2P_BASE_BASIC_MDNS_RESPONDER_H_

#include <map>
#include <set>
#include <string>

#include "p2p/base/mdns_message.h"
#include "p2p/base/mdns_responder.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/bytebuffer.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// All methods of BasicMDnsResponder should be called on the same thread. This
// is checked by |thread_checker_|.
class BasicMDnsResponder : public MDnsResponder, public ::sigslot::has_slots<> {
 public:
  // The listening sockets should join the group of mDNS multicast and bound to
  // port 5353.
  BasicMDnsResponder(rtc::AsyncPacketSocket* listen_socket_ipv4,
                     rtc::AsyncPacketSocket* listen_socket_ipv6,
                     rtc::AsyncPacketSocket* send_socket);
  ~BasicMDnsResponder() override;

  std::string CreateNameForAddress(const rtc::IPAddress& address) override;
  void OnQueryReceived(uint16_t query_id,
                       const rtc::SocketAddress& from,
                       const std::set<std::string>& names_to_resolve,
                       bool prefer_unicast_response) override;
  void OnResponseReadyToSend(
      uint16_t response_id,
      const rtc::SocketAddress& to,
      const std::map<std::string, rtc::IPAddress>& resolution) override;
  void OnResponseSent(rtc::AsyncPacketSocket* socket,
                      const rtc::SentPacket& packet);

  sigslot::signal0<> SignalResponseSent;

 private:
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_address,
                    const rtc::PacketTime& packet_time);

  rtc::AsyncPacketSocket* listen_socket_ipv4_;
  rtc::AsyncPacketSocket* listen_socket_ipv6_;
  rtc::AsyncPacketSocket* send_socket_;
  rtc::ThreadChecker thread_checker_;
  std::map<rtc::IPAddress, std::string> name_by_ip_;
  int64_t last_time_response_sent_ = 0;
  rtc::AsyncInvoker invoker_;
};

}  // namespace webrtc

#endif  // P2P_BASE_BASIC_MDNS_RESPONDER_H_
