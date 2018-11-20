/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/video_frame_stash.h"

#include <utility>

namespace webrtc {
namespace video_coding {

VideoFrameStash::VideoFrameStash(Container::size_type capacity)
    : capacity_(capacity) {}

void VideoFrameStash::StashFrame(
    std::unique_ptr<RtpFrameObject> frame_to_stash) {
  if (stashed_frames_.size() >= capacity_) {
    stashed_frames_.pop_back();
  }
  stashed_frames_.push_front(std::move(frame_to_stash));
}

VideoFrameStash::Container::iterator VideoFrameStash::RemoveFrame(
    Container::iterator frame_to_remove) {
  return stashed_frames_.erase(frame_to_remove);
}

VideoFrameStash::Container::size_type VideoFrameStash::GetCapacity() const
    noexcept {
  return capacity_;
}

VideoFrameStash::Container::iterator VideoFrameStash::begin() noexcept {
  return stashed_frames_.begin();
}

VideoFrameStash::Container::iterator VideoFrameStash::end() noexcept {
  return stashed_frames_.end();
}

VideoFrameStash::Container::const_iterator VideoFrameStash::begin() const
    noexcept {
  return stashed_frames_.begin();
}

VideoFrameStash::Container::const_iterator VideoFrameStash::end() const
    noexcept {
  return stashed_frames_.end();
}

VideoFrameStash::Container::size_type VideoFrameStash::size() const {
  return stashed_frames_.size();
}

}  // namespace video_coding
}  // namespace webrtc
