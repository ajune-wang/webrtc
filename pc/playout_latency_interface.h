/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PLAYOUT_LATENCY_INTERFACE_H_
#define PC_PLAYOUT_LATENCY_INTERFACE_H_

#include <stdint.h>

#include "media/base/delayable.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// "Interface" suffix in the interface name is required to be compatible with
// webrtc/api/proxy.cc
class PlayoutLatencyInterface : public rtc::RefCountInterface {
 public:
  virtual void OnStart(cricket::Delayable* media_channel, uint32_t ssrc) = 0;

  virtual void OnStop() = 0;

  virtual void SetLatency(double latency) = 0;

  virtual double GetLatency() const = 0;
};

}  // namespace webrtc

#endif  // PC_PLAYOUT_LATENCY_INTERFACE_H_
