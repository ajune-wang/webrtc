
/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_ECN_MARKING_H_
#define API_TRANSPORT_ECN_MARKING_H_
namespace rtc {

// TODO(https://bugs.webrtc.org/15368): L4S support is slowly being developed.
// Help is appreciated.

// L4S Explicit Congestion Notification (ECN) .
// https://www.rfc-editor.org/rfc/rfc9331.html ECT stands for ECN-Capable
// Transport and CE stands for Congestion Experienced.
enum class EcnMarking { kNotECT = 0, kECTOne = 1, kCE = 3 };

}  // namespace rtc

#endif  // API_TRANSPORT_ECN_MARKING_H_
