/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_SOCKETERROR_H_
#define P2P_BASE_SOCKETERROR_H_

#include <ostream>

namespace rtc {

// Used to allow socket errors (as returned by socket "GetError" methods) to be
// conveniently logged.
// TODO(deadbeef): Change socket classes to return this directly, instead of
// returning an int?
class SocketError {
 public:
  explicit SocketError(int code) : code_(code) {}
  int code() const { return code_; }

 private:
  int code_;
};

// Prints the error as "<code> (<description>)", or just "<code>".
std::ostream& operator<<(std::ostream& stream, const SocketError& error);

}  // namespace rtc

#endif  // P2P_BASE_SOCKETERROR_H_
