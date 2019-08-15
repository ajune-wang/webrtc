/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_SVC_RATE_ALLOCATOR_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_SVC_RATE_ALLOCATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {

class SvcRateAllocator : public VideoBitrateAllocator {
 public:
  explicit SvcRateAllocator(const VideoCodec& codec);

  VideoBitrateAllocation Allocate(
      VideoBitrateAllocationParameters parameters) override;

  static DataRate GetMaxBitrate(const VideoCodec& codec);
  static DataRate GetPaddingBitrate(const VideoCodec& codec);

 private:
  VideoBitrateAllocation GetAllocationNormalVideo(
      DataRate total_bitrate,
      DataRate stable_bitrate,
      size_t num_spatial_layers) const;

  VideoBitrateAllocation GetAllocationScreenSharing(
      DataRate total_bitrate,
      DataRate stable_bitrate,
      size_t num_spatial_layers) const;

  const VideoCodec codec_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP9_SVC_RATE_ALLOCATOR_H_
