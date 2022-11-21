/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEO_VIDEO_FRAME_READER_H_
#define API_TEST_VIDEO_VIDEO_FRAME_READER_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/video/video_frame.h"

namespace webrtc {
namespace test {

class VideoFrameReader {
 public:
  virtual ~VideoFrameReader() = default;

  // Reads frame specified by `frame_num`. Returns `absl::nullopt` if
  // `frame_num` points outside of stream or a reading error occured.
  virtual absl::optional<VideoFrame> ReadFrame(size_t frame_num) = 0;

  // Closes writer and cleans up all resources. No invocations to `ReadFrame`
  // are allowed after `Close` was invoked.
  virtual void Close() = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_VIDEO_FRAME_READER_H_
