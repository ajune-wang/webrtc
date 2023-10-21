/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_STATS_IMPL_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_STATS_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/video_codec_stats.h"

namespace webrtc {
namespace test {

// Implementation of `VideoCodecStats`. This class is not thread-safe.
class VideoCodecStatsImpl : public VideoCodecStats {
 public:
  void AddFrame(const Frame& frame);

  // Returns raw pointers to previously added frame. If frame does not exist,
  // returns `nullptr`.
  Frame* GetFrame(uint32_t timestamp_rtp, int spatial_idx);

  std::vector<Frame> Slice(Filter filter = Filter{},
                           bool merge = false) const override;

  Stream Aggregate(Filter filter = Filter{}) const override;

 protected:
  struct FrameId {
    uint32_t timestamp_rtp;
    int spatial_idx;

    bool operator==(const FrameId& o) const {
      return timestamp_rtp == o.timestamp_rtp && spatial_idx == o.spatial_idx;
    }

    bool operator<(const FrameId& o) const {
      if (timestamp_rtp < o.timestamp_rtp)
        return true;
      if (timestamp_rtp == o.timestamp_rtp && spatial_idx < o.spatial_idx)
        return true;
      return false;
    }
  };

  std::vector<Frame> Merge(
      const std::vector<Frame>& frames,
      absl::optional<LayerId> target_layer_id = absl::nullopt) const;

  std::map<FrameId, Frame> frames_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_STATS_IMPL_H_
