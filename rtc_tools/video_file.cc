/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/video_file.h"

namespace webrtc {
namespace test {

VideoFile::Iterator::Iterator(const rtc::scoped_refptr<const VideoFile>& video,
                              size_t index)
    : video_(video), index_(index) {}

VideoFile::Iterator::Iterator(const VideoFile::Iterator& other) = default;
VideoFile::Iterator::Iterator(VideoFile::Iterator&& other) = default;
VideoFile::Iterator& VideoFile::Iterator::operator=(VideoFile::Iterator&&) =
    default;
VideoFile::Iterator& VideoFile::Iterator::operator=(
    const VideoFile::Iterator&) = default;
VideoFile::Iterator::~Iterator() = default;

rtc::scoped_refptr<I420BufferInterface> VideoFile::Iterator::operator*() const {
  return video_->GetFrame(index_);
}
bool VideoFile::Iterator::operator==(const VideoFile::Iterator& other) const {
  return index_ == other.index_;
}
bool VideoFile::Iterator::operator!=(const VideoFile::Iterator& other) const {
  return !(*this == other);
}

VideoFile::Iterator VideoFile::Iterator::operator++(int) {
  const Iterator copy = *this;
  ++*this;
  return copy;
}

VideoFile::Iterator& VideoFile::Iterator::operator++() {
  ++index_;
  return *this;
}

VideoFile::Iterator VideoFile::begin() const {
  return Iterator(this, 0);
}

VideoFile::Iterator VideoFile::end() const {
  return Iterator(this, number_of_frames());
}

}  // namespace test
}  // namespace webrtc
