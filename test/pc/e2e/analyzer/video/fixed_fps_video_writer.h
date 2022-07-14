/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_FIXED_FPS_VIDEO_WRITER_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_FIXED_FPS_VIDEO_WRITER_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/video/video_sink_interface.h"
#include "system_wrappers/include/clock.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Writes video to the specified video writer with specified fixed frame rate.
// If at the point in time X no new frames are passed to the writer, the
// previous frame is used to fill the gap and preserve frame rate.
//
// It uses next algorithm:
// A - expected interframe interval for requested frame rate.
//
// Option I.
// ========
// New frame arrived after last frame with interval > A.
//
// first frame
// |
// |     last frame                 position BEFORE
// |     |                            |
// |-----|-----+-----+-----+-----+----+--X--+----------
//             |     |     |     |    |  ^  |
//           expected, but missed frames |  position AFTER
//                                       |
//                                       next received frame after
//                                       freeze (let's call it X)
//
// Then if (X arrival time - last expected frame time) < A / 2, then X
// will be placed on position "BEFORE" in the output, otherwise it will be
// placed on position "AFTER".
//
// Option II.
// =========
// New frame arrived after last frame with interval < A.
//
// first frame
// |
// |               last frame   expected frame in the future
// |                       |     |
// |-----|-----|-----|-----|--X--+----------
//                            ^  |
//                            |  position AFTER
//                            |
//                            next received frame (let's call it X)
//
// Then if (X arrival time - last frame time) < A / 2, then X will replace
// the last frame, otherwise it will be placed on position "AFTER".
//
// Option III.
// ==========
// New frame arrived before last frame with interval < A. It may happen when
// position "AFTER" was selected for options I or II.
//
// first frame
// |
// |                           last frame
// |                             |
// |-----|-----|-----|-----|--X--|----------
//                            ^
//                            |
//                            next received frame (let's call it X)
//
// Then if (X arrival time - last frame time) < A / 2, then X will replace
// the last frame, otherwise error will be thrown.
class FixedFpsVideoWriter : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FixedFpsVideoWriter(Clock* clock,
                      test::VideoFrameWriter* video_writer,
                      int fps);
  ~FixedFpsVideoWriter() override;

  void OnFrame(const VideoFrame& frame) override;

 private:
  void WriteFrame(absl::optional<VideoFrame> frame);
  Timestamp Now() const;

  const TimeDelta inter_frame_interval_;
  Clock* const clock_;
  test::VideoFrameWriter* const video_writer_;

  Timestamp last_frame_time_ = Timestamp::MinusInfinity();
  absl::optional<VideoFrame> last_frame_ = absl::nullopt;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_FIXED_FPS_VIDEO_WRITER_H_
