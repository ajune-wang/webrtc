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
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {
namespace {
using RepeatMode = YuvFrameReaderImpl::RepeatMode;

int WrapFrameNum(int frame_num, int num_frames, RepeatMode mode) {
  RTC_CHECK_GE(frame_num, 0) << "frame_num cannot be negative";
  RTC_CHECK_GE(num_frames, 0) << "num_frames cannot be negative";
  if (mode == RepeatMode::kSingle) {
    return frame_num;
  }
  if (mode == RepeatMode::kRepeat) {
    return frame_num % num_frames;
  }

  RTC_CHECK_EQ(RepeatMode::kPingPong, mode);
  int cycle_len = 2 * (num_frames - 1);
  int wrapped_num = frame_num % cycle_len;
  if (wrapped_num >= num_frames) {
    return cycle_len - wrapped_num;
  }
  return wrapped_num;
}

rtc::scoped_refptr<I420Buffer> Scale(rtc::scoped_refptr<I420Buffer> buffer,
                                     int desired_width,
                                     int desired_height) {
  if (buffer->width() == desired_width && buffer->height() == desired_height) {
    return buffer;
  }
  rtc::scoped_refptr<I420Buffer> scaled(
      I420Buffer::Create(desired_width, desired_height));
  scaled->ScaleFrom(*buffer.get());
  return scaled;
}
}  // namespace

int YuvFrameReaderImpl::RateScaler::Skip(int base_rate, int target_rate) {
  ticks_ = ticks_.value_or(base_rate);
  int skip = 0;
  while (*ticks_ < base_rate) {
    *ticks_ += target_rate;
    ++skip;
  }
  *ticks_ -= base_rate;
  return skip;
}

YuvFrameReaderImpl::YuvFrameReaderImpl(std::string filepath,
                                       int width,
                                       int height,
                                       RepeatMode repeat_mode)
    : filepath_(filepath),
      width_(width),
      height_(height),
      repeat_mode_(repeat_mode),
      num_frames_(0),
      frame_num_(0),
      frame_size_bytes_(0),
      header_size_bytes_(0),
      file_(nullptr) {}

YuvFrameReaderImpl::~YuvFrameReaderImpl() {
  if (file_ != nullptr) {
    fclose(file_);
    file_ = nullptr;
  }
}

void YuvFrameReaderImpl::Init() {
  RTC_CHECK_GT(width_, 0) << "Width must be positive";
  RTC_CHECK_GT(height_, 0) << "Height must be positive";
  int luma_wxh = width_ * height_;
  int chroma_wxh = ((width_ + 1) / 2) * ((height_ + 1) / 2);
  frame_size_bytes_ = luma_wxh + 2 * chroma_wxh;

  file_ = fopen(filepath_.c_str(), "rb");
  RTC_CHECK(file_ != NULL) << "Cannot open " << filepath_;

  size_t file_size_bytes = GetFileSize(filepath_);
  RTC_CHECK_GT(file_size_bytes, 0u) << "File " << filepath_ << " is empty";

  num_frames_ = static_cast<int>(file_size_bytes / frame_size_bytes_);
  RTC_CHECK_GT(num_frames_, 0u) << "File " << filepath_ << " is too small";
}

rtc::scoped_refptr<I420Buffer> YuvFrameReaderImpl::PullFrame() {
  return PullFrame(/*frame_num=*/nullptr);
}

rtc::scoped_refptr<I420Buffer> YuvFrameReaderImpl::PullFrame(int* frame_num) {
  return PullFrame(frame_num, /*desired_width=*/width_,
                   /*desired_height=*/height_,
                   /*base_framerate=*/1,
                   /*desired_framerate=*/1);
}

rtc::scoped_refptr<I420Buffer> YuvFrameReaderImpl::PullFrame(
    int* frame_num,
    int desired_width,
    int desired_height,
    int base_framerate,
    int desired_framerate) {
  frame_num_ += framerate_scaler_.Skip(base_framerate, desired_framerate);
  auto buffer = ReadFrame(frame_num_, desired_width, desired_height);
  if (frame_num != nullptr) {
    *frame_num = frame_num_;
  }
  return buffer;
}

rtc::scoped_refptr<I420Buffer> YuvFrameReaderImpl::ReadFrame(int frame_num) {
  return ReadFrame(frame_num, /*desired_width=*/width_,
                   /*desired_height=*/height_);
}

rtc::scoped_refptr<I420Buffer> YuvFrameReaderImpl::ReadFrame(
    int frame_num,
    int desired_width,
    int desired_height) {
  int wrapped_num = WrapFrameNum(frame_num, num_frames_, repeat_mode_);
  if (wrapped_num >= num_frames_) {
    RTC_CHECK_EQ(RepeatMode::kSingle, repeat_mode_);
    return nullptr;
  }
  fseek(file_, header_size_bytes_ + wrapped_num * frame_size_bytes_, SEEK_SET);
  auto buffer = ReadI420Buffer(width_, height_, file_);
  RTC_CHECK(buffer != nullptr);

  return Scale(buffer, desired_width, desired_height);
}

std::unique_ptr<FrameReader> CreateYuvFrameReader(std::string filepath,
                                                  int width,
                                                  int height) {
  return CreateYuvFrameReader(filepath, width, height,
                              YuvFrameReaderImpl::RepeatMode::kSingle);
}

std::unique_ptr<FrameReader> CreateYuvFrameReader(
    std::string filepath,
    int width,
    int height,
    YuvFrameReaderImpl::RepeatMode repeat_mode) {
  YuvFrameReaderImpl* frame_reader =
      new YuvFrameReaderImpl(filepath, width, height, repeat_mode);
  frame_reader->Init();
  return std::unique_ptr<FrameReader>(frame_reader);
}

}  // namespace test
}  // namespace webrtc
