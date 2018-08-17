/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_TOOLS_Y4M_FILE_READER_H_
#define RTC_TOOLS_Y4M_FILE_READER_H_

#include <fstream>
#include <string>
#include <vector>

#include "rtc_base/sequenced_task_checker.h"
#include "rtc_tools/video_file.h"

namespace webrtc {
namespace test {

class Y4mFile : public VideoFile {
 public:
  // This function opens the file and reads it as an .y4m file. It returns null
  // on failure. The file will be closed when the returned object is destroyed.
  static rtc::scoped_refptr<Y4mFile> Open(const std::string& file_name);

  size_t number_of_frames() const override;

  rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t frame_index) const override;

  int width() const;
  int height() const;
  float fps() const;

 protected:
  Y4mFile(int width,
          int height,
          float fps,
          const std::vector<std::streampos>& frame_positions,
          std::ifstream&& file);
  ~Y4mFile() override;

 private:
  const int width_;
  const int height_;
  const float fps_;
  const std::vector<std::streampos> frame_positions_;
  const rtc::SequencedTaskChecker thread_checker_;
  // This file has to be marked mutable because GetFrame() is const. What we
  // mutate is the file position, but we always reset that to an absolute number
  // before doing anything else, so the file is in const regardless.
  mutable std::ifstream file_;
};

}  // namespace test
}  // namespace webrtc

#endif  // RTC_TOOLS_Y4M_FILE_READER_H_
