/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_MDNS_RESPONDER_H_
#define P2P_BASE_MDNS_RESPONDER_H_

#include <map>
#include <set>
#include <string>

#include "p2p/base/packetsocketfactory.h"
#include "rtc_base/ipaddress.h"
#include "rtc_base/socketaddress.h"

namespace webrtc {

class MDnsResponder {
 public:
  MDnsResponder() = default;
  virtual ~MDnsResponder() = default;
  // Creates a type-4 UUID hostname for an IP address if there is no cached
  // name for this address, or retrieves the cached name.
  virtual std::string CreateNameForAddress(const rtc::IPAddress& address) = 0;
  // Called when an mDNS query is received on port 5353 from an mDNS multicast
  // group, namely 224.0.0.251 or ff02::fb. If the query contains names that we
  // have created and/or announced in the subnet(s), we should prepare and send
  // an mDNS response for these names.
  virtual void OnQueryReceived(uint16_t query_id,
                               const rtc::SocketAddress& from,
                               const std::set<std::string>& names_to_resolve,
                               bool unicast_response) = 0;
  // Called when the name resolution is done and approved by a response rate
  // limiter. The remote address |to| should be either an mDNS multicast address
  // or a unicast address if the corresponding query sets the unicast-response
  // bit.
  virtual void OnResponseReadyToSend(
      uint16_t response_id,
      const rtc::SocketAddress& to,
      const std::map<std::string, rtc::IPAddress>& resolution) = 0;
};

}  // namespace webrtc

#endif  // P2P_BASE_MDNS_RESPONDER_H_
