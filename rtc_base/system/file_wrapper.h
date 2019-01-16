/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_FILE_WRAPPER_H_
#define RTC_BASE_SYSTEM_FILE_WRAPPER_H_

#include <stddef.h>
#include <stdio.h>

#include "rtc_base/critical_section.h"

// Implementation that can read (exclusive) or write from/to a file.

namespace webrtc {

// TODO(tommi): Rename to rtc::File and move to base.
class FileWrapper final {
 public:
  // Opens a file in read or write mode, decided by the read_only parameter.
  static FileWrapper Open(const char* file_name_utf8, bool read_only);

  FileWrapper();

  explicit FileWrapper(FILE* file);
  ~FileWrapper();

  // Support for move semantics.
  FileWrapper(FileWrapper&& other);
  FileWrapper& operator=(FileWrapper&& other);

  // Returns true if a file has been opened.
  // TODO(nisse): Support implicit conversion to bool?
  bool is_open() const { return file_ != nullptr; }

  void Close();

  // Flush any pending writes.  Note: Flushing when closing, is not required.
  bool Flush();

  // Rewinds the file to the start.
  bool Rewind();
  int Read(void* buf, size_t length);
  bool Write(const void* buf, size_t length);

 private:
  FILE* file_ = nullptr;

  // Copying is not supported.
  FileWrapper(const FileWrapper&) = delete;
  FileWrapper& operator=(const FileWrapper&) = delete;
};

}  // namespace webrtc

#endif  // RTC_BASE_SYSTEM_FILE_WRAPPER_H_
