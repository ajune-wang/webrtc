/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEO_TEST_VIDEO_TRACK_SOURCE_H_
#define API_TEST_VIDEO_TEST_VIDEO_TRACK_SOURCE_H_

#include "pc/video_track_source.h"

namespace webrtc {
namespace test {

// Video source that can be used as input for tests.
class TestVideoTrackSource : public VideoTrackSource {
 public:
  explicit TestVideoTrackSource(bool remote) : VideoTrackSource(remote) {}
  virtual ~TestVideoTrackSource() = default;

  // Starts producing video.
  virtual void Start() = 0;

  // Stops producing video.
  virtual void Stop() = 0;

  virtual void SetScreencast(bool is_screencast) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_TEST_VIDEO_TRACK_SOURCE_H_
