/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/active_decode_target_sender.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

void ActiveDecodeTargetSender::OnFrame(
    const FrameDependencyStructure& video_structure,
    const std::vector<bool>& active_decode_targets,
    bool is_keyframe,
    std::vector<bool> part_of_chain) {
  RTC_DCHECK_LE(video_structure.num_decode_targets, 32);
  RTC_DCHECK_LE(video_structure.num_chains, 32);
  if (!active_decode_targets.empty()) {
    RTC_DCHECK_EQ(active_decode_targets.size(),
                  video_structure.num_decode_targets);
  }
  RTC_DCHECK_EQ(part_of_chain.size(), video_structure.num_chains);

  if (is_keyframe) {
    // Key frame resets all the important state.
    last_active_decode_targets_bitmask_ = ~uint32_t{0};
    unsent_on_chain_bitmask_ = 0;
  } else {
    // On delta frame update state assuming previous frame was sent.
    // Technically it could be called right after that frame was send, but since
    // state should be queried until there is a new frame to send,
    // it is postponed until now. That makes interface cleaner.
    OnPostSend();
  }
  // Save for the next OnPostSend processing.
  last_part_of_chain_ = std::move(part_of_chain);

  // Set 1 in the unused bits of the active_decode_target_bitmask.
  // This way value for 'all targets are active' would always be -1 regardless
  // of number of decode targets.
  // This way it is also implicetly treat empty active decode targets as all
  // are active and thus has implicit backward compatibility with structures
  // that never deactive any decode targets.
  uint32_t active_decode_targets_bitmask = ~uint32_t{0};
  for (size_t dt = 0; dt < active_decode_targets.size(); ++dt) {
    if (!active_decode_targets[dt]) {
      active_decode_targets_bitmask &= ~(uint32_t{1} << dt);
    }
  }
  if (active_decode_targets_bitmask == last_active_decode_targets_bitmask_) {
    return;
  }
  last_active_decode_targets_bitmask_ = active_decode_targets_bitmask;
  unsent_on_chain_bitmask_ = 0;
  if (video_structure.num_chains == 0) {
    // Chains feature is not used, but active decode targets are set.
    // Some other reliability mechanic should be implemented for this case.
    // It is not implemented until needed.
    RTC_LOG(LS_WARNING) << "Chains are not enabled. (In)active decode taregets "
                           "will not be send reliably.";
    unsent_on_chain_bitmask_ = 1;
    // Clear the unsent_on_chain_bitmask_ on the next frame.
    last_part_of_chain_ = {true};
  }

  // calculate list of active chains. It is likely frames that are part of
  // inactive would never by produces, and are not expected by the remote.
  for (int dt = 0; dt < video_structure.num_decode_targets; ++dt) {
    if (!active_decode_targets[dt]) {
      continue;
    }
    int chain_idx = video_structure.decode_target_protected_by_chain[dt];
    // chain_idx == video_structure.num_chains is valid and means the
    // decode target is not protected by any chain.
    if (chain_idx < video_structure.num_chains) {
      unsent_on_chain_bitmask_ |= (uint32_t{1} << chain_idx);
    }
  }
}

void ActiveDecodeTargetSender::OnPostSend() {
  if (unsent_on_chain_bitmask_ == 0) {
    return;
  }
  for (size_t i = 0; i < last_part_of_chain_.size(); ++i) {
    if (last_part_of_chain_[i]) {
      unsent_on_chain_bitmask_ &= ~(uint32_t{1} << i);
    }
  }
}

}  // namespace webrtc
