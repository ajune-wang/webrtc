/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PLAYOUT_LATENCY_H_
#define PC_PLAYOUT_LATENCY_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "media/base/delayable.h"
#include "pc/playout_latency_interface.h"
#include "rtc_base/thread.h"

namespace webrtc {

class PlayoutLatency : public PlayoutLatencyInterface {
 public:
  // Must be called on signaling thread.
  explicit PlayoutLatency(rtc::Thread* worker_thread);

  void OnStart(cricket::Delayable* media_channel, uint32_t ssrc) override;

  void OnStop() override;

  void SetLatency(double latency) override;

  double GetLatency() const override;

 private:
  // Signaling thread.
  rtc::Thread* const main_thread_;
  rtc::Thread* const worker_thread_;
  // Media channel and ssrc together uniqely identify audio stream.
  cricket::Delayable* media_channel_ = nullptr;
  absl::optional<uint32_t> ssrc_;
  absl::optional<double> cached_latency_;
};

}  // namespace webrtc

#endif  // PC_PLAYOUT_LATENCY_H_
