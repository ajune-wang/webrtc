/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/active_decode_targets_helper.h"

#include <stdint.h>

#include <vector>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

uint32_t ToBitmask(const std::vector<bool>& bool_vector) {
  RTC_DCHECK_LE(bool_vector.size(), 32);
  uint32_t result = 0;
  for (size_t i = 0; i < bool_vector.size(); ++i) {
    if (bool_vector[i]) {
      result |= (uint32_t{1} << i);
    }
  }
  return result;
}

// Returns bitmask for `num_decode_targets` decode targets when all are active.
uint32_t AllActiveBitmask(size_t num_decode_targets) {
  RTC_DCHECK_LE(num_decode_targets, 32);
  return (~uint32_t{0}) >> (32 - num_decode_targets);
}

}  // namespace

void ActiveDecodeTargetsHelper::OnFrame(
    rtc::ArrayView<const int> decode_target_protected_by_chain,
    const std::vector<bool>& active_decode_targets,
    bool is_keyframe,
    const std::vector<bool>& frame_is_part_of_chain) {
  const size_t num_decode_targets = decode_target_protected_by_chain.size();
  if (!active_decode_targets.empty()) {
    RTC_DCHECK_EQ(active_decode_targets.size(), num_decode_targets);
  }
  if (is_keyframe) {
    // Key frame resets the state.
    last_active_decode_targets_bitmask_ = AllActiveBitmask(num_decode_targets);
    unsent_on_chain_bitmask_ = 0;
  } else {
    // Update state assuming previous frame was sent.
    unsent_on_chain_bitmask_ &= ~last_sent_on_chain_bitmask_;
  }
  // Save for the next call to OnFrame.
  last_sent_on_chain_bitmask_ = ToBitmask(frame_is_part_of_chain);

  uint32_t active_decode_targets_bitmask =
      active_decode_targets.empty() ? AllActiveBitmask(num_decode_targets)
                                    : ToBitmask(active_decode_targets);
  if (active_decode_targets_bitmask == last_active_decode_targets_bitmask_) {
    return;
  }
  last_active_decode_targets_bitmask_ = active_decode_targets_bitmask;
  const int num_chains = frame_is_part_of_chain.size();

  // Calculate list of active chains. Frames that are part of inactive chains
  // should not be expected.
  unsent_on_chain_bitmask_ = 0;
  for (size_t dt = 0; dt < num_decode_targets; ++dt) {
    if (dt < active_decode_targets.size() && !active_decode_targets[dt]) {
      continue;
    }
    int chain_idx = decode_target_protected_by_chain[dt];
    // chain_idx == num_chains is valid and means the decode target is
    // not protected by any chain.
    if (chain_idx < num_chains) {
      unsent_on_chain_bitmask_ |= (uint32_t{1} << chain_idx);
    }
  }

  if (unsent_on_chain_bitmask_ == 0) {
    // Active decode targets are not protected by any chains,
    // e.g. chains are not used.
    // Some other reliability mechanic should be implemented for this case.
    RTC_LOG(LS_WARNING)
        << "Active decode targets protected by no chains. (In)active decode "
           "targets information will not be send reliably.";
    // Set non-zero value to send the active_decode_targets_bitmask (once).
    unsent_on_chain_bitmask_ = 1;
    // Clear the unsent_on_chain_bitmask_ on the next frame to send
    // active_decode_targets_bitmask only once.
    last_sent_on_chain_bitmask_ = 1;
  }
}

}  // namespace webrtc
