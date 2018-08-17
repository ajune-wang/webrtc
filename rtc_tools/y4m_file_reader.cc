/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/y4m_file_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace test {

Video::Iterator::Iterator(const rtc::scoped_refptr<const Video>& video,
                          size_t index)
    : video_(video), index_(index) {}

Video::Iterator::Iterator(const Video::Iterator& other) = default;
Video::Iterator::Iterator(Video::Iterator&& other) = default;
Video::Iterator& Video::Iterator::operator=(Video::Iterator&&) = default;
Video::Iterator& Video::Iterator::operator=(const Video::Iterator&) = default;
Video::Iterator::~Iterator() = default;

rtc::scoped_refptr<I420BufferInterface> Video::Iterator::operator*() const {
  return video_->GetFrame(index_);
}
bool Video::Iterator::operator==(const Video::Iterator& other) const {
  return index_ == other.index_;
}
bool Video::Iterator::operator!=(const Video::Iterator& other) const {
  return !(*this == other);
}

Video::Iterator Video::Iterator::operator++(int) {
  const Iterator copy = *this;
  ++*this;
  return copy;
}

Video::Iterator& Video::Iterator::operator++() {
  ++index_;
  return *this;
}

Video::Iterator Video::begin() const {
  return Iterator(this, 0);
}

Video::Iterator Video::end() const {
  return Iterator(this, number_of_frames());
}

rtc::scoped_refptr<Y4mFile> Y4mFile::Open(const std::string& file_name) {
  std::ifstream file(file_name);
  if (!file.is_open()) {
    RTC_LOG(LS_ERROR) << "Could not open input file for reading: " << file_name;
    return nullptr;
  }

  std::string header_line;
  std::getline(file, header_line);
  const char Y4M_FILE_HEADER[] = "YUV4MPEG2";
  if (!rtc::starts_with(header_line.c_str(), Y4M_FILE_HEADER)) {
    RTC_LOG(LS_ERROR) << "File " << file_name << " does not start with "
                      << Y4M_FILE_HEADER << " header";
    return nullptr;
  }

  absl::optional<int> width;
  absl::optional<int> height;
  absl::optional<float> fps;

  std::vector<std::string> fields;
  rtc::tokenize(header_line, ' ', &fields);
  for (const std::string& field : fields) {
    const char prefix = field.front();
    const std::string suffix = field.substr(1);
    switch (prefix) {
      case 'W':
        width = rtc::StringToNumber<int>(suffix);
        break;
      case 'H':
        height = rtc::StringToNumber<int>(suffix);
        break;
      case 'C':
        if (suffix != "420" && suffix != "420mpeg2") {
          RTC_LOG(LS_ERROR)
              << "Does not support any other color space than I420 or "
                 "420mpeg2, but was: "
              << suffix;
          return nullptr;
        }
        break;
      case 'F': {
        std::vector<std::string> fraction;
        rtc::tokenize(suffix, ':', &fraction);
        if (fraction.size() == 2) {
          const absl::optional<int> numerator =
              rtc::StringToNumber<int>(fraction[0]);
          const absl::optional<int> denominator =
              rtc::StringToNumber<int>(fraction[1]);
          if (numerator && denominator && *denominator != 0)
            fps = *numerator / static_cast<float>(*denominator);
          break;
        }
      }
    }
  }
  if (!width || !height) {
    RTC_LOG(LS_ERROR) << "Could not find width and height in file header";
    return nullptr;
  }
  if (!fps) {
    RTC_LOG(LS_ERROR) << "Could not find fps in file header";
    return nullptr;
  }
  RTC_LOG(LS_INFO) << "Video has resolution: " << *width << "x" << *height
                   << " " << *fps << " fps";
  if (*width % 2 != 0 || *height % 2 != 0) {
    RTC_LOG(LS_ERROR)
        << "Only supports even width/height so that chroma size is a "
           "whole number.";
    return nullptr;
  }

  const std::streampos i420_frame_size = 3 * *width * *height / 2;
  std::vector<std::streampos> frame_positions;
  while (file.peek() != EOF) {
    std::getline(file, header_line);
    if (!rtc::starts_with(header_line.c_str(), "FRAME")) {
      RTC_LOG(LS_ERROR) << "Did not find FRAME header on line: \""
                        << header_line << '\"' << ", ignoring rest of file";
      break;
    }
    frame_positions.push_back(file.tellg());
    // Skip over YUV pixel data.
    file.seekg(i420_frame_size, std::ios_base::cur);
    if (!file.good()) {
      RTC_LOG(LS_ERROR) << "Could not skip past the YUV data for frame number: "
                        << frame_positions.size();
    }
  }
  if (frame_positions.empty()) {
    RTC_LOG(LS_ERROR) << "Could not find any frames in the file";
    return nullptr;
  }
  RTC_LOG(LS_INFO) << "Video has " << frame_positions.size() << " frames";

  return new rtc::RefCountedObject<Y4mFile>(*width, *height, *fps,
                                            frame_positions, std::move(file));
}

size_t Y4mFile::number_of_frames() const {
  return frame_positions_.size();
}

rtc::scoped_refptr<I420BufferInterface> Y4mFile::GetFrame(
    size_t frame_index) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&thread_checker_);

  file_.seekg(frame_positions_.at(frame_index));
  rtc::scoped_refptr<I420Buffer> buffer = I420Buffer::Create(width_, height_);
  file_.read(reinterpret_cast<char*>(buffer->MutableDataY()), width_ * height_);
  file_.read(reinterpret_cast<char*>(buffer->MutableDataU()),
             buffer->ChromaWidth() * buffer->ChromaHeight());
  file_.read(reinterpret_cast<char*>(buffer->MutableDataV()),
             buffer->ChromaWidth() * buffer->ChromaHeight());

  if (!file_.good()) {
    RTC_LOG(LS_ERROR) << "Could not ready YUV data for frame " << frame_index;
    return nullptr;
  }
  return buffer;
}

int Y4mFile::width() const {
  return width_;
}

int Y4mFile::height() const {
  return height_;
}

float Y4mFile::fps() const {
  return fps_;
}

Y4mFile::Y4mFile(int width,
                 int height,
                 float fps,
                 const std::vector<std::streampos>& frame_positions,
                 std::ifstream&& file)
    : width_(width),
      height_(height),
      fps_(fps),
      frame_positions_(frame_positions),
      file_(std::move(file)) {}

Y4mFile::~Y4mFile() = default;

}  // namespace test
}  // namespace webrtc
