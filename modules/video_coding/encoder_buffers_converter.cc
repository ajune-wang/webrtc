/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/encoder_buffers_converter.h"

#include <stdint.h>

#include <iterator>
#include <set>

#include "absl/algorithm/container.h"
#include "absl/container/inlined_vector.h"
#include "api/array_view.h"
#include "api/video/video_frame_type.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace webrtc {

absl::InlinedVector<int64_t, 5> EncoderBuffersConverter::CalculateDependencies(
    VideoFrameType frame_type,
    int64_t frame_id,
    rtc::ArrayView<const CodecBufferUsage> buffers_usage) {
  RTC_DCHECK_RUN_ON(&checker_);

  absl::InlinedVector<int64_t, 5> dependencies;
  // Calculate depenendencies based on buffer usage.
  RTC_DCHECK_GT(buffers_usage.size(), 0);
  if (buffers_.size() < buffers_usage.size()) {
    buffers_.resize(buffers_usage.size());
  }
  std::set<int64_t> direct_depenendencies;
  std::set<int64_t> indirect_depenendencies;
  if (frame_type == VideoFrameType::kVideoFrameDelta) {
    for (size_t i = 0; i < buffers_usage.size(); ++i) {
      if (!buffers_usage[i].referenced) {
        continue;
      }
      const BufferUsage& buffer = buffers_[i];
      if (buffer.frame_id == absl::nullopt) {
        RTC_LOG(LS_ERROR) << "Odd configuration: frame " << frame_id
                          << " references buffer # " << i
                          << " that was never updated.";
        continue;
      }
      direct_depenendencies.insert(*buffer.frame_id);
      indirect_depenendencies.insert(buffer.dependencies.begin(),
                                     buffer.dependencies.end());
    }
    // Reduce references: if frame #3 depends on frame #2 and #1, and frame #2
    // depends on frame #1, then frame #3 needs to depend just on frame #2.
    // Though this set diff removes only 1 level of indirection, it seems
    // enough for all currently used structures.
    absl::c_set_difference(direct_depenendencies, indirect_depenendencies,
                           std::back_inserter(dependencies));
  }

  // Update buffers.
  for (size_t i = 0; i < buffers_usage.size(); ++i) {
    if (!buffers_usage[i].updated) {
      continue;
    }
    BufferUsage& buffer = buffers_[i];
    buffer.frame_id = frame_id;
    buffer.dependencies.assign(direct_depenendencies.begin(),
                               direct_depenendencies.end());
  }

  return dependencies;
}

}  // namespace webrtc
