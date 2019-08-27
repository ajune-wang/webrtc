/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_OUTPUT_STREAM_H_
#define API_OUTPUT_STREAM_H_

#include <cstddef>

namespace webrtc {

// NOTE: This class is still under development and may change without notice.
class OutputStream {
 public:
  virtual ~OutputStream() = default;

  // Write data to the output stream. Returns true if the data was
  // successfully written in its entirety. Otherwise, false is returned and
  // there's no guarantee how much data was written, if any. The
  // output sink becomes inactive after the first time |false| is returned.
  // This call is a no-op if the output stream is inactive.
  virtual bool Write(const void* data, std::size_t size) = 0;
};

// NOTE: This class is still under development and may change without notice.
class RewindableOutputStream : public OutputStream {
 public:
  // Rewinds the output stream to the beginning. Returns true if the operation
  // was successful.
  virtual bool Rewind() = 0;
};

}  // namespace webrtc

#endif  // API_OUTPUT_STREAM_H_
