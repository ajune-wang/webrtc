/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ACTIVE_DECODE_TARGETS_HELPER_H_
#define MODULES_RTP_RTCP_SOURCE_ACTIVE_DECODE_TARGETS_HELPER_H_

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"

namespace webrtc {

// Helper class that decides when active_decode_target_bitmask should be written
// into the dependency descriptor rtp header extension.
// See: https://aomediacodec.github.io/av1-rtp-spec/#a44-switching
// This class is thread-compatible
class ActiveDecodeTargetsHelper {
 public:
  ActiveDecodeTargetsHelper() = default;
  ActiveDecodeTargetsHelper(const ActiveDecodeTargetsHelper&) = delete;
  ActiveDecodeTargetsHelper& operator=(const ActiveDecodeTargetsHelper&) =
      delete;
  ~ActiveDecodeTargetsHelper() = default;

  // Decides if active decode target bitmask should be attached to the frame
  // that is about to be sent.
  void OnFrame(rtc::ArrayView<const int> decode_target_protected_by_chain,
               const std::vector<bool>& active_decode_targets,
               bool is_keyframe,
               const std::vector<bool>& frame_is_part_of_chain);

  // Returns active decode target to attach to the dependency descriptor.
  absl::optional<uint32_t> ActiveDecodeTargetsBitmask() const {
    if (unsent_on_chain_bitmask_ == 0)
      return absl::nullopt;
    return last_active_decode_targets_bitmask_;
  }

 private:
  // `(unsent_on_chain_bitmask_ & (1 << i)) != 0` indicates last active decode
  // target bitmask wasn't attached to a packet on the chain with id `i`.
  uint32_t unsent_on_chain_bitmask_ = 0;
  uint32_t last_active_decode_targets_bitmask_ = 0;
  // Bitmask of the indexes of the chains last frame is part of.
  uint32_t last_sent_on_chain_bitmask_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_ACTIVE_DECODE_TARGETS_HELPER_H_
