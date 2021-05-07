/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/null_transport.h"

namespace webrtc {
namespace test {

void NullTransport::SendRtp(const uint8_t* packet,
                            size_t length,
                            const PacketOptions& options) {}

void NullTransport::SendRtcp(const uint8_t* packet, size_t length) {}

}  // namespace test
}  // namespace webrtc
