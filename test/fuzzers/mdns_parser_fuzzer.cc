/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include "p2p/base/mdns_message.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  MDnsMessage::MessageInfo info;
  info.message_start = reinterpret_cast<const char*>(data);
  info.message_size = size;
  MDnsMessageBufferReader buf(info);
  std::unique_ptr<MDnsMessage> mdns_msg(new MDnsMessage());
  mdns_msg->Read(&buf);
}

}  // namespace webrtc
