/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_VIDEO_FRAME_STASH_H_
#define MODULES_VIDEO_CODING_VIDEO_FRAME_STASH_H_

#include <deque>
#include <memory>

#include "modules/video_coding/frame_object.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {
namespace video_coding {

// The VideoFrameStash is responsible for storing recieved video frames for a
// short peroid of time that either cannot currently be decrypted or do not
// have all the information to determine their references. This is intended to
// be a very limited cache utilized by the FrameDecryptor and the
// FrameReferenceFinder as a temporary store.
// Note: This class is not thread safe as it requires iteration over elements.
// Please provide your own locking mechanism to ensure single access.
class VideoFrameStash final {
 public:
  // The internal container type used to store the stashed frames.
  typedef std::deque<std::unique_ptr<RtpFrameObject>> Container;
  // Constructs a new VideoFrameStash object with an explict maximum capacity.
  // If frames are added past this earlier frames will be removed to make room
  // for them (ring buffer).
  explicit VideoFrameStash(Container::size_type capacity);
  // This object is not copyable as it holds a container of unique_ptrs.
  VideoFrameStash(const VideoFrameStash&) = delete;
  VideoFrameStash& operator=(const VideoFrameStash&) = delete;
  // Stashes the frame at the front of the container. If the container is at
  // capacity the last element in the container is removed and this element is
  // prepended to the container.
  void StashFrame(std::unique_ptr<RtpFrameObject> frame_to_stash);
  // Removes the stashed frame pointed to by the iterator. Since this will
  // invalidate any internal iterator it returns the next element after the
  // erased element.
  Container::iterator RemoveFrame(Container::iterator frame_to_remove);
  // Returns the maximum capacity set during construction.
  Container::size_type GetCapacity() const noexcept;
  // Returns an iterator to the beginning of the container.
  Container::iterator begin() noexcept;
  // Returns an iterator to the end of the container.
  Container::iterator end() noexcept;
  // Returns a const_iterator to the beginning of the container.
  Container::const_iterator begin() const noexcept;
  // Returns a const_iteroator to the end of the container.
  Container::const_iterator end() const noexcept;
  // Returns the current number of elements in the VideoFrameStash.
  Container::size_type size() const;

 private:
  // The maximum capacity of the container.
  const Container::size_type capacity_;
  // Frames that have been fully received but didn't have all the information
  // needed to determine their references or are not yet decryptable.
  Container stashed_frames_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_VIDEO_FRAME_STASH_H_
