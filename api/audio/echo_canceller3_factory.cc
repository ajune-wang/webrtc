/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/echo_canceller3_factory.h"

#include <memory>

#include "modules/audio_processing/aec3/echo_canceller3.h"

namespace webrtc {

EchoCanceller3Factory::EchoCanceller3Factory() {}

EchoCanceller3Factory::EchoCanceller3Factory(const EchoCanceller3Config& config)
    : config_(config) {
  // Revert to default configuration if needed.
  if (!EchoCanceller3::Validate(config_)) {
    config_ = EchoCanceller3Config();
  }
}

std::unique_ptr<EchoControl> EchoCanceller3Factory::Create(int sample_rate_hz) {
  return std::unique_ptr<EchoControl>(
      new EchoCanceller3(config_, sample_rate_hz, true));
}
}  // namespace webrtc
