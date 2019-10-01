/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/neteq_controller.h"

#include "modules/audio_coding/neteq/packet_buffer.h"

namespace webrtc {

NetEqFacade::NetEqFacade() = default;
NetEqFacade::~NetEqFacade() = default;

bool NetEqFacade::IsObsoleteTimestamp(uint32_t timestamp,
                                      uint32_t timestamp_limit,
                                      uint32_t horizon_samples) {
  return PacketBuffer::IsObsoleteTimestamp(timestamp, timestamp_limit,
                                           horizon_samples);
}

NetEqController::NetEqController() = default;
NetEqController::~NetEqController() = default;

NetEqControllerFactory::NetEqControllerFactory() = default;
NetEqControllerFactory::~NetEqControllerFactory() = default;

}  // namespace webrtc
