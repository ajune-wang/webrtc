/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_FRAME_READER_H_
#define TEST_TESTSUPPORT_FRAME_READER_H_

#include <stdio.h>

#include <string>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"

namespace webrtc {
class I420Buffer;
namespace test {

// Handles reading of I420 frames from video files.
class FrameReader {
 public:
  virtual ~FrameReader() {}

  // Reads and returns next frame. Returns `nullptr` if reading failed or end of
  // stream is reached.
  virtual rtc::scoped_refptr<I420Buffer> PullFrame() = 0;

  // Reads and returns next frame. `frame_num` stores unwrapped frame number
  // which can be passed to `ReadFrame` to re-read this frame latter. Returns
  // `nullptr` if reading failed or end of stream is reached.
  virtual rtc::scoped_refptr<I420Buffer> PullFrame(int* frame_num) = 0;

  // Reads and returns frame specified by `frame_num`. Returns `nullptr` if
  // reading failed.
  virtual rtc::scoped_refptr<I420Buffer> ReadFrame(int frame_num) = 0;

  // Reads next frame, scales and returns it. `frame_num` stores unwrapped frame
  // number which can be passed to `ReadFrame` to re-read this frame latter.
  // Returns `nullptr` if reading failed.
  virtual rtc::scoped_refptr<I420Buffer> PullFrame(int* frame_num,
                                                   int desired_width,
                                                   int desired_height,
                                                   int base_framerate,
                                                   int desired_framerate) = 0;

  // Reads frame specified by `frame_num`, scales and returns it. Returns
  // `nullptr` if reading failed.
  virtual rtc::scoped_refptr<I420Buffer> ReadFrame(int frame_num,
                                                   int desired_width,
                                                   int desired_height) = 0;

  // Total number of frames in file.
  virtual int num_frames() const = 0;
};

class YuvFrameReaderImpl : public FrameReader {
 public:
  enum class RepeatMode { kSingle, kRepeat, kPingPong };

  class RateScaler {
   public:
    int Skip(int base_rate, int target_rate);

   private:
    absl::optional<int> ticks_;
  };

  // Creates a file handler. The input file is assumed to exist and be readable.
  // Parameters:
  //   filepath                Path to file to read from.
  //   width, height           Size of each frame to read.
  //   repeat_mode             Repeat mode.
  YuvFrameReaderImpl(std::string filepath,
                     int width,
                     int height,
                     RepeatMode repeat_mode);

  ~YuvFrameReaderImpl() override;

  virtual void Init();

  rtc::scoped_refptr<I420Buffer> PullFrame() override;

  rtc::scoped_refptr<I420Buffer> PullFrame(int* frame_num) override;

  rtc::scoped_refptr<I420Buffer> PullFrame(int* frame_num,
                                           int desired_width,
                                           int desired_height,
                                           int base_framerate,
                                           int desired_framerate) override;

  rtc::scoped_refptr<I420Buffer> ReadFrame(int frame_num) override;

  rtc::scoped_refptr<I420Buffer> ReadFrame(int frame_num,
                                           int desired_width,
                                           int desired_height) override;

  int num_frames() const override { return num_frames_; }

 protected:
  const std::string filepath_;
  int width_;
  int height_;
  RepeatMode repeat_mode_;
  int num_frames_;
  int frame_num_;
  int frame_size_bytes_;
  int header_size_bytes_;
  FILE* file_;
  RateScaler framerate_scaler_;
};

class Y4mFrameReaderImpl : public YuvFrameReaderImpl {
 public:
  // Creates a file handler. The input file is assumed to exist and be readable.
  // Parameters:
  //   filepath                Path to file to read from.
  //   repeat_mode             Repeat mode.
  Y4mFrameReaderImpl(std::string filepath, RepeatMode repeat_mode);

  void Init() override;
};

std::unique_ptr<FrameReader> CreateYuvFrameReader(std::string filepath,
                                                  int widht,
                                                  int height);

std::unique_ptr<FrameReader> CreateYuvFrameReader(
    std::string filepath,
    int widht,
    int height,
    YuvFrameReaderImpl::RepeatMode repeat_mode);

std::unique_ptr<FrameReader> CreateY4mFrameReader(std::string filepath);

std::unique_ptr<FrameReader> CreateY4mFrameReader(
    std::string filepath,
    YuvFrameReaderImpl::RepeatMode repeat_mode);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_FRAME_READER_H_
