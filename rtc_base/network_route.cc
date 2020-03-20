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

std::string NetworkRoute::DebugString() const {
#ifndef RTC_DISABLE_LOGGING  // Remove method if compiling w/o logging.
  rtc::StringBuilder oss;
  oss << "[ connected: " << connected << " local: [ " << local.adapter_id()
      << "/" << local.network_id() << " "
      << AdapterTypeToString(local.adapter_type())
      << " turn: " << local.uses_turn() << " ] remote: [ "
      << remote.adapter_id() << "/" << remote.network_id() << " "
      << AdapterTypeToString(remote.adapter_type())
      << " turn: " << remote.uses_turn()
      << " ] packet_overhead_bytes: " << packet_overhead << " ]";
  return oss.Release();
#else
  return "";
#endif
}

}  // namespace rtc
