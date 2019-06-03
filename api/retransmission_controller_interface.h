/* Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is an EXPERIMENTAL interface for controlling retransmission.

#ifndef API_RETRANSMISSION_CONTROLLER_INTERFACE_H_
#define API_RETRANSMISSION_CONTROLLER_INTERFACE_H_

namespace webrtc {

// Interface for enabling/disabling retransmissions on a media stream.
// Control whether retransmissions should be used for a given stream. This is
// useful when an encoder decides to handle losses based on a different type
// of feedback message.
class RetransmissionControllerInterface {
 public:
  virtual void DisableRetransmission() = 0;
  virtual void EnableRetransmission() = 0;

 protected:
  virtual ~RetransmissionControllerInterface() = default;
};

}  // namespace webrtc

#endif  // API_RETRANSMISSION_CONTROLLER_INTERFACE_H_
