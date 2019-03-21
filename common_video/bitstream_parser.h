/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_BITSTREAM_PARSER_H_
#define COMMON_VIDEO_BITSTREAM_PARSER_H_
#include <stddef.h>
#include <stdint.h>

namespace webrtc {

// This class is an interface for bitstream parsers.
class BitstreamParser {
 public:
  enum Result {
    kOk,
    kInvalidStream,
    kUnsupportedStream,
  };
  virtual ~BitstreamParser() = default;

  // Parse an additional chunk of the bitstream.
  virtual void ParseBitstream(const uint8_t* bitstream, size_t length) = 0;

  // Get the last extracted QP value from the parsed bitstream.
  virtual bool GetLastSliceQp(int* qp) const = 0;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_BITSTREAM_PARSER_H_
