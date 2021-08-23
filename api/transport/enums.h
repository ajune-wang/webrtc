/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
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

// See https://w3c.github.io/webrtc-pc/#rtcicetransportstate
// Note that kFailed is currently not a terminal state, and a transport might
// incorrectly be marked as failed while gathering candidates, see
// bugs.webrtc.org/8833
enum class IceTransportState {
  kNew,
  kChecking,
  kConnected,
  kCompleted,
  kFailed,
  kDisconnected,
  kClosed,
};

enum PortPrunePolicy {
  NO_PRUNE,                 // Do not prune.
  PRUNE_BASED_ON_PRIORITY,  // Prune lower-priority ports on the same network.
  KEEP_FIRST_READY          // Keep the first ready port and prune the rest
                            // on the same network.
};

enum class VpnPreference {
  DEFAULT,        // No VPN preference.
  ONLY_USE_VPN,   // only use VPN connections.
  NEVER_USE_VPN,  // never use VPN connections
  PREFER_VPN,     // use a VPN connection if possible, i.e VPN connections sorts
                  // higher than all other connections.
  AVOID_VPN,      // only use VPN if there is no other connections, i.e VPN
                  // connections sorts last.
};

}  // namespace webrtc

#endif  // API_TRANSPORT_ENUMS_H_
