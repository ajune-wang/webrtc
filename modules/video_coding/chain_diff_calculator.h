/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CHAIN_DIFF_CALCULATOR_H_
#define MODULES_VIDEO_CODING_CHAIN_DIFF_CALCULATOR_H_

#include <stdint.h>

#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/video/video_frame_type.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

namespace webrtc {

// This class is thread compatible.
class ChainDiffCalculator {
 public:
  ChainDiffCalculator() = default;
  ChainDiffCalculator(const ChainDiffCalculator&) = default;
  ChainDiffCalculator& operator=(const ChainDiffCalculator&) = default;

  void ResetAll(int num_chains) {
    last_frame_in_chain_.assign(num_chains, absl::nullopt);
  }

  // Calculates chain diffs based on flags if frame is part of the chain
  absl::InlinedVector<int, 4> From(int64_t frame_id,
                                   const std::vector<bool>& chains);
  // Calcaulates chain diff for a frame that is not part of any.
  absl::InlinedVector<int, 4> From(int64_t frame_id) const;

 private:
  absl::InlinedVector<absl::optional<int64_t>, 4> last_frame_in_chain_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CHAIN_DIFF_CALCULATOR_H_
