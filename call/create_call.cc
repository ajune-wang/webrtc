/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/create_call.h"

#include <memory>

#include "call/call.h"

namespace webrtc {

std::unique_ptr<Call> CreateCall(const CallConfig& config) {
  return Call::Create(config);
}

}  // namespace webrtc
