/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_YUV_FRAME_READER_H_
#define TEST_TESTSUPPORT_YUV_FRAME_READER_H_

#include <stdio.h>

#include <string>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/test/video/video_frame_reader.h"

namespace webrtc {
class I420Buffer;
namespace test {

class YuvFrameReader {
 public:
  virtual ~YuvFrameReader() = default;

  enum class RepeatMode { kRepeat, kPingPong };

  virtual rtc::scoped_refptr<I420Buffer> PullFrame() {
    return PullFrame(/*pulled_frame_num=*/nullptr);
  }

  virtual rtc::scoped_refptr<I420Buffer> PullFrame(int* pulled_frame_num) = 0;

  virtual rtc::scoped_refptr<I420Buffer> ReadFrame(int frame_num) = 0;

  virtual rtc::scoped_refptr<I420Buffer> PullFrame(int* pulled_frame_num,
                                                   int desired_width,
                                                   int desired_height,
                                                   int base_framerate,
                                                   int desired_framerate) = 0;

  virtual rtc::scoped_refptr<I420Buffer> ReadFrame(int frame_num,
                                                   int desired_width,
                                                   int desired_height) = 0;

  // Frame length in bytes of a single frame image.
  virtual int FrameSizeBytes() = 0;

  // Total number of frames in the input video source.
  virtual int NumberOfFrames() = 0;
};

std::unique_ptr<YuvFrameReader> CreateYuvFrameReader(
    std::string filepath,
    int width,
    int height,
    YuvFrameReader::RepeatMode repeat_mode);

inline std::unique_ptr<YuvFrameReader>
CreateYuvFrameReader(std::string filepath, int width, int height) {
  return CreateYuvFrameReader(filepath, width, height,
                              YuvFrameReader::RepeatMode::kPingPong);
}

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_FRAME_READER_H_