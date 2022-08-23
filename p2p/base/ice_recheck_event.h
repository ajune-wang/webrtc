/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_RECHECK_EVENT_H_
#define P2P_BASE_ICE_RECHECK_EVENT_H_

#include <string>

#include "p2p/base/ice_switch_reason.h"

namespace cricket {

struct IceRecheckEvent {
  IceRecheckEvent(IceSwitchReason _reason, int _recheck_delay_ms)
      : reason(_reason), recheck_delay_ms(_recheck_delay_ms) {}

  std::string ToString() const;

  IceSwitchReason reason;
  int recheck_delay_ms;
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_RECHECK_EVENT_H_
