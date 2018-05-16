/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICETRANSPORTSTATS_H_
#define P2P_BASE_ICETRANSPORTSTATS_H_

namespace webrtc {

struct IceTransportStats {
  bool writable;
  bool receiving;
  int num_continual_switchings_to_weak_candidate_pairs;
  int selected_candidate_pair_connectivity_check_rtt_ms;
};

}  // namespace webrtc

#endif  // P2P_BASE_ICETRANSPORTSTATS_H_
