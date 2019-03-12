/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>

#include "modules/rtp_rtcp/include/rtp_header_parser.h"

namespace webrtc {

// Fuzz s_url_decode which is used in ice server parsing.
void FuzzOneInput(const uint8_t* data, size_t size) {
  RtpHeaderParser::IsRtcp(data, size);
  RtpHeaderParser::GetSsrc(data, size);
  RTPHeader rtp_header;
  std::unique_ptr<RtpHeaderParser> rtp_header_parser(RtpHeaderParser::Create());
  rtp_header_parser->Parse(data, size, &rtp_header);
}

}  // namespace webrtc
