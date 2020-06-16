/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ACTIVE_DECODE_TARGET_SENDER_H_
#define MODULES_RTP_RTCP_SOURCE_ACTIVE_DECODE_TARGET_SENDER_H_

#include <stdint.h>

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"

namespace webrtc {

// Helper class that decides when active_decode_target_bitmask should be written
// in the dependency descriptor.
// See: https://aomediacodec.github.io/av1-rtp-spec/#a44-switching
// This class is thread-compatible
class ActiveDecodeTargetSender {
 public:
  ActiveDecodeTargetSender() = default;
  ActiveDecodeTargetSender(const ActiveDecodeTargetSender&) = delete;
  ActiveDecodeTargetSender& operator=(const ActiveDecodeTargetSender&) = delete;

  // Decides if active decode target bitmask should be attached based on the
  // frame that is about to be sent.
  void OnFrame(const FrameDependencyStructure& video_structure,
               const std::vector<bool>& active_decode_targets,
               bool is_keyframe,
               std::vector<bool> part_of_chain);

  // Returns active decode target to attache to the dependency descriptor.
  absl::optional<uint32_t> ActiveDecodeTargetBitmask() const {
    if (unsent_on_chain_bitmask_ == 0)
      return absl::nullopt;
    return last_active_decode_targets_bitmask_;
  }

 private:
  void OnPostSend();

  // (unsent_on_chain_ & (1 << i)) != 0  when last active decode target bitmask
  // wasn't attach to an packet on the chain with id `i`.
  uint32_t unsent_on_chain_bitmask_ = 0;
  uint32_t last_active_decode_targets_bitmask_ = ~uint32_t{0};
  std::vector<bool> last_part_of_chain_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_ACTIVE_DECODE_TARGET_SENDER_H_
