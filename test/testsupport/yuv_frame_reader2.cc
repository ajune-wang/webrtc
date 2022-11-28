/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <string>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/logging.h"
#include "test/frame_utils.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/yuv_frame_reader.h"

namespace webrtc {
namespace test {

namespace {
class RateScaler {
 public:
  int Skip(int base_rate, int target_rate) {
    samples_ = samples_.value_or(base_rate);
    int skip = 0;
    while (*samples_ < base_rate) {
      *samples_ += target_rate;
      ++skip;
    }
    *samples_ -= base_rate;
    return skip;
  }

 private:
  absl::optional<int> samples_;
};
}  // namespace

class YuvFrameReaderImpl2 : public YuvFrameReader {
 public:
  YuvFrameReaderImpl2(std::string filepath,
                      int width,
                      int height,
                      RepeatMode repeat_mode)
      : width_(width),
        height_(height),
        repeat_mode_(repeat_mode),
        num_frames_(0),
        frame_num_(0),
        file_(nullptr) {
    int luma_wxh = width * height;
    int chroma_wxh = ((width + 1) / 2) * ((height + 1) / 2);
    frame_size_bytes_ = luma_wxh + 2 * chroma_wxh;

    RTC_CHECK_GT(width_, 0) << "Width must be positive";
    RTC_CHECK_GT(height_, 0) << "Height must be positive";

    file_ = fopen(filepath.c_str(), "rb");
    RTC_CHECK(file_ != nullptr) << "Cannot open " << filepath;

    size_t file_size_bytes = GetFileSize(filepath);
    RTC_CHECK_GT(file_size_bytes, 0u) << "File " << filepath << " is empty";

    num_frames_ = static_cast<int>(file_size_bytes / frame_size_bytes_);
    RTC_CHECK_GT(num_frames_, 0u) << "File " << filepath << " is too small";
  }

  ~YuvFrameReaderImpl2() {
    if (file_ != nullptr) {
      fclose(file_);
      file_ = nullptr;
    }
  }

  rtc::scoped_refptr<I420Buffer> PullFrame(int* pulled_frame_num) override {
    return PullFrame(pulled_frame_num, /*desired_width=*/width_,
                     /*desired_height=*/height_,
                     /*base_framerate=*/1,
                     /*desired_framerate=*/1);
  }

  rtc::scoped_refptr<I420Buffer> PullFrame(int* pulled_frame_num,
                                           int desired_width,
                                           int desired_height,
                                           int base_framerate,
                                           int desired_framerate) override {
    frame_num_ += framerate_scaler_.Skip(base_framerate, desired_framerate);
    auto buffer = ReadFrame(frame_num_, desired_width, desired_height);
    RTC_CHECK(buffer != nullptr);

    if (pulled_frame_num != nullptr) {
      *pulled_frame_num = frame_num_;
    }

    return buffer;
  }

  rtc::scoped_refptr<I420Buffer> ReadFrame(int frame_num) override {
    return ReadFrame(frame_num, /*desired_width=*/width_,
                     /*desired_height=*/height_);
  }

  rtc::scoped_refptr<I420Buffer> ReadFrame(int frame_num,
                                           int desired_width,
                                           int desired_height) override {
    int wrapped_num = WrapFrameNum(frame_num);
    fseek(file_, wrapped_num * frame_size_bytes_, SEEK_SET);

    auto buffer = ReadI420Buffer(width_, height_, file_);
    RTC_CHECK(buffer != nullptr);

    return Scale(buffer, desired_width, desired_height);
  }

  int FrameSizeBytes() override { return frame_size_bytes_; }

  int NumberOfFrames() override { return num_frames_; }

 protected:
  int WrapFrameNum(int frame_num) {
    RTC_CHECK_GE(frame_num, 0) << "frame_num cannot be negative";
    if (repeat_mode_ == RepeatMode::kRepeat) {
      return frame_num % num_frames_;
    }

    // Ping-pong.
    int cycle_len = 2 * (num_frames_ - 1);
    int wrapped_num = frame_num % cycle_len;
    if (wrapped_num >= num_frames_) {
      return cycle_len - wrapped_num;
    }
    return wrapped_num;
  }

  rtc::scoped_refptr<I420Buffer> Scale(rtc::scoped_refptr<I420Buffer> buffer,
                                       int desired_width,
                                       int desired_height) {
    if (buffer->width() == desired_width &&
        buffer->height() == desired_height) {
      return buffer;
    }

    rtc::scoped_refptr<I420Buffer> scaled(
        I420Buffer::Create(desired_width, desired_height));
    scaled->ScaleFrom(*buffer.get());
    return scaled;
  }

  int width_;
  int height_;
  RepeatMode repeat_mode_;
  int num_frames_;
  int frame_num_;
  FILE* file_;
  int frame_size_bytes_;
  RateScaler framerate_scaler_;
};

std::unique_ptr<YuvFrameReader> CreateYuvFrameReader(
    std::string filepath,
    int width,
    int height,
    YuvFrameReader::RepeatMode repeat_mode) {
  return std::make_unique<YuvFrameReaderImpl2>(filepath, width, height,
                                               repeat_mode);
}

}  // namespace test
}  // namespace webrtc
