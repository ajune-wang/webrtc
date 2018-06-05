/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICETRANSPORTSTATS_H_
#define P2P_BASE_ICETRANSPORTSTATS_H_

#include "p2p/base/port.h"

namespace webrtc {

struct IceTransportStats {
  IceTransportStats();
  ~IceTransportStats();
  int last_time_ms_candidate_pairs_sorted = 0;
  int num_active_candidate_pairs = 0;
  int num_writable_candidate_pairs = 0;
  int num_continual_switchings_to_weak_candidate_pairs = 0;
  bool had_selected_candidate_pair = false;
  bool selected_candidate_pair_writable_or_presumably_writable = true;
  bool selected_candidate_pair_receiving = false;
  int selected_candidate_pair_connectivity_check_rtt_ms = -1;
  cricket::ConnectionInfos candidate_pair_stats_list;
  cricket::CandidateStatsList candidate_stats_list;
};

}  // namespace webrtc

#endif  // P2P_BASE_ICETRANSPORTSTATS_H_
