/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_RTP_REF_FINDER_UNITTEST_HELPER_H_
#define MODULES_VIDEO_CODING_RTP_REF_FINDER_UNITTEST_HELPER_H_

#include <memory>
#include <vector>

#include "api/video/encoded_frame.h"
#include "test/gmock.h"

namespace webrtc {

class RtpRefFinderTestHelper {
 public:
  static ::testing::Matcher<
      const std::vector<std::unique_ptr<video_coding::EncodedFrame>>&>
  HasFrameWithIdAndRefs(int64_t frame_id, std::vector<int64_t> refs);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_RTP_REF_FINDER_UNITTEST_HELPER_H_
