/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_RTP_FILE_READER_H_
#define TEST_RTP_FILE_READER_H_

#include <set>
#include <string>

#include "common_types.h"  // NOLINT(build/include)
#include "test/rtp_packet.h"

namespace webrtc {
namespace test {

class RtpFileReader {
 public:
  enum FileFormat { kPcap, kRtpDump, kLengthPacketInterleaved };
  virtual ~RtpFileReader() = default;
  static RtpFileReader* Create(FileFormat format, const std::string& filename);
  static RtpFileReader* Create(FileFormat format,
                               const std::string& filename,
                               const std::set<uint32_t>& ssrc_filter);
  virtual bool NextPacket(RtpPacket* packet) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_RTP_FILE_READER_H_
