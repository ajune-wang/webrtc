/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/network_route.h"

#include "rtc_base/strings/string_builder.h"

namespace rtc {

std::string NetworkRoute::ToString() const {
  rtc::StringBuilder oss;
  oss << "[ connected: " << connected << " local: [ " << local.adapter_id << "/"
      << local.network_id << " " << AdapterTypeToString(local.adapter_type)
      << " relay: " << local.relay << " ] remote: [ " << remote.adapter_id
      << "/" << remote.network_id << " "
      << AdapterTypeToString(remote.adapter_type) << " relay: " << remote.relay
      << " ] packet_overhead_bytes: " << packet_overhead << " rtt: " << rtt_ms
      << " +/- " << rtt_ms_confidence << " ]";
  return oss.Release();
}

}  // namespace rtc
