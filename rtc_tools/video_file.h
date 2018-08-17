/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_TOOLS_VIDEO_FILE_H_
#define RTC_TOOLS_VIDEO_FILE_H_

#include <iterator>

#include "api/video/video_frame.h"
#include "rtc_base/refcount.h"

namespace webrtc {
namespace test {

// Iterable class representing a sequence of I420 buffers. This class is not
// thread safe because it is expected to be backed by a file.
class VideoFile : public rtc::RefCountInterface {
 public:
  class Iterator {
   public:
    typedef int value_type;
    typedef std::ptrdiff_t difference_type;
    typedef int* pointer;
    typedef int& reference;
    typedef std::input_iterator_tag iterator_category;

    Iterator(const rtc::scoped_refptr<const VideoFile>& video, size_t index);
    Iterator(const Iterator& other);
    Iterator(Iterator&& other);
    Iterator& operator=(Iterator&&);
    Iterator& operator=(const Iterator&);
    ~Iterator();

    rtc::scoped_refptr<I420BufferInterface> operator*() const;
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

    Iterator operator++(int);
    Iterator& operator++();

   private:
    rtc::scoped_refptr<const VideoFile> video_;
    size_t index_;
  };

  Iterator begin() const;
  Iterator end() const;

  virtual size_t number_of_frames() const = 0;
  virtual rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t index) const = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // RTC_TOOLS_VIDEO_FILE_H_
