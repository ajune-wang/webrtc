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

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// Returns mask of ids of chains previous frame is part of.
// Assumes for each chain frames are seen in order and no frame on any chain is
// missing. That assumptions allows a simple detection when previous frame is
// part of a chain.
std::bitset<32> LastSendOnChain(int frame_diff,
                                rtc::ArrayView<const int> chain_diffs) {
  std::bitset<32> bitmask = 0;
  for (size_t i = 0; i < chain_diffs.size(); ++i) {
    if (frame_diff == chain_diffs[i]) {
      bitmask.set(i);
    }
  }
  return bitmask;
}

// Returns bitmask for `num_decode_targets` decode targets when all are active.
std::bitset<32> AllDecodeTargets(size_t num_decode_targets) {
  RTC_DCHECK_LE(num_decode_targets, 32);
  return (~uint32_t{0}) >> (32 - num_decode_targets);
}

std::bitset<32> ActiveChains(
    rtc::ArrayView<const int> decode_target_protected_by_chain,
    int num_chains,
    std::bitset<32> active_decode_targets) {
  std::bitset<32> active_chains = 0;
  for (size_t dt = 0; dt < decode_target_protected_by_chain.size(); ++dt) {
    if (dt < active_decode_targets.size() && !active_decode_targets[dt]) {
      continue;
    }
    // chain_idx == num_chains is valid and means the decode target is
    // not protected by any chain.
    int chain_idx = decode_target_protected_by_chain[dt];
    if (chain_idx < num_chains) {
      active_chains.set(chain_idx);
    }
  }
  return active_chains;
}

}  // namespace

void ActiveDecodeTargetsHelper::OnFrame(
    rtc::ArrayView<const int> decode_target_protected_by_chain,
    std::bitset<32> active_decode_targets,
    bool is_keyframe,
    int64_t frame_id,
    rtc::ArrayView<const int> chain_diffs) {
  const size_t num_decode_targets = decode_target_protected_by_chain.size();
  const int num_chains = chain_diffs.size();
  std::bitset<32> all_decode_targets = AllDecodeTargets(num_decode_targets);
  // Ignore bits of the decode targes that do not exist.
  active_decode_targets &= all_decode_targets;

  if (is_keyframe) {
    // Key frame resets the state.
    last_active_decode_targets_ = all_decode_targets;
    last_active_chains_ = AllDecodeTargets(num_chains);
    unsent_on_chain_.reset();
  } else {
    // Update state assuming previous frame was sent.
    unsent_on_chain_ &=
        ~LastSendOnChain(frame_id - last_frame_id_, chain_diffs);
  }
  // Save for the next call to OnFrame.
  // Though usually `frame_id == last_frame_id_ + 1`, it might not be so when
  // frame id space is shared by several simulcast rtp streams.
  last_frame_id_ = frame_id;

  if (active_decode_targets == last_active_decode_targets_) {
    return;
  }
  last_active_decode_targets_ = active_decode_targets;
  last_active_chains_ = ActiveChains(decode_target_protected_by_chain,
                                     num_chains, active_decode_targets);
  // Calculate list of active chains. Frames that are part of inactive chains
  // should not be expected.
  unsent_on_chain_ = last_active_chains_;
  if (unsent_on_chain_.none()) {
    // Active decode targets are not protected by any chains,
    // e.g. chains are not used.
    // To be on the safe side always send the active_decode_targets_bitmask
    // from now on.
    RTC_LOG(LS_WARNING)
        << "Active decode targets protected by no chains. (In)active decode "
           "targets information will be send overreliably.";
    unsent_on_chain_.set(1);
  }
}

}  // namespace webrtc
