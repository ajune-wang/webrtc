/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NETWORK_ROUTE_H_
#define RTC_BASE_NETWORK_ROUTE_H_

#include <stdint.h>

#include <string>

#include "rtc_base/network_constants.h"

// TODO(honghaiz): Make a directory that describes the interfaces and structs
// the media code can rely on and the network code can implement, and both can
// depend on that, but not depend on each other. Then, move this file to that
// directory.
namespace rtc {

struct RouteEndpoint {
  AdapterType adapter_type = ADAPTER_TYPE_UNKNOWN;
  uint16_t adapter_id = 0;
  uint16_t network_id = 0;
  bool relay = false;
};

struct NetworkRoute {
  bool connected = false;
  RouteEndpoint local;
  RouteEndpoint remote;
  // Last packet id sent on the PREVIOUS route.
  int last_sent_packet_id = -1;
  // The overhead in bytes from IP layer and above.
  // This is the maximum of any part of the route.
  int packet_overhead = 0;

  // Downstream projects depend on the old representation,
  // populate that until they have been migrated.
  // TODO(jonaso): remove.
  uint16_t local_network_id = 0;
  uint16_t remote_network_id = 0;

  std::string ToString() const;
};

inline bool operator==(const RouteEndpoint& a, const RouteEndpoint& b) {
  return a.adapter_type == b.adapter_type && a.adapter_id == b.adapter_id &&
         a.network_id == b.network_id && a.relay == b.relay;
}

inline bool operator==(const NetworkRoute& a, const NetworkRoute& b) {
  return a.connected == b.connected && a.local == b.local &&
         a.remote == b.remote && a.packet_overhead == b.packet_overhead;
}

}  // namespace rtc

#endif  // RTC_BASE_NETWORK_ROUTE_H_
