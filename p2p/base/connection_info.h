/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_CONNECTION_INFO_H_
#define P2P_BASE_CONNECTION_INFO_H_

// TODO(bugs.webrtc.org/10647): Delete this header when downstream is updated.
#include "p2p/base/connection_stats.h"

namespace cricket {

// Some clients use the internal ConnectionInfo-class.
// Add a typedef for those until we clean them up.
typedef ConnectionStats ConnectionInfo;

}  // namespace cricket

#endif  // P2P_BASE_CONNECTION_INFO_H_
