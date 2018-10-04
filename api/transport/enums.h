/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_ENUMS_H_
#define API_TRANSPORT_ENUMS_H_
namespace webrtc {
enum IceTransportState {
  kIceTransportNew,
  kIceTransportChecking,
  kIceTransportConnected,
  kIceTransportCompleted,
  kIceTransportFailed,
  kIceTransportDisconnected,
  kIceTransportClosed,
};
}  // namespace webrtc

#endif  // API_TRANSPORT_ENUMS_H_
