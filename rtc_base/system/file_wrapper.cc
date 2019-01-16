/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/system/file_wrapper.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <string.h>
#endif

#include <utility>

namespace webrtc {
namespace {
FILE* FileOpen(const char* file_name_utf8, bool read_only) {
#if defined(_WIN32)
  int len = MultiByteToWideChar(CP_UTF8, 0, file_name_utf8, -1, nullptr, 0);
  std::wstring wstr(len, 0);
  MultiByteToWideChar(CP_UTF8, 0, file_name_utf8, -1, &wstr[0], len);
  FILE* file = _wfopen(wstr.c_str(), read_only ? L"rb" : L"wb");
#else
  FILE* file = fopen(file_name_utf8, read_only ? "rb" : "wb");
#endif
  return file;
}
}  // namespace

// static
FileWrapper FileWrapper::Open(const char* file_name_utf8, bool read_only) {
  return FileWrapper(FileOpen(file_name_utf8, read_only));
}

FileWrapper::FileWrapper() {}

FileWrapper::FileWrapper(FILE* file) : file_(file) {}

FileWrapper::~FileWrapper() {
  Close();
}

FileWrapper::FileWrapper(FileWrapper&& other) {
  operator=(std::move(other));
}

FileWrapper& FileWrapper::operator=(FileWrapper&& other) {
  file_ = other.file_;
  other.file_ = nullptr;
  return *this;
}

bool FileWrapper::Rewind() {
  if (!file_)
    return false;
  return fseek(file_, 0, SEEK_SET) == 0;
}

bool FileWrapper::Flush() {
  if (!file_)
    return false;
  return fflush(file_) == 0;
}

int FileWrapper::Read(void* buf, size_t length) {
  if (file_ == nullptr)
    return -1;

  size_t bytes_read = fread(buf, 1, length, file_);
  return static_cast<int>(bytes_read);
}

bool FileWrapper::Write(const void* buf, size_t length) {
  if (buf == nullptr)
    return false;

  if (file_ == nullptr)
    return false;

  return fwrite(buf, 1, length, file_) == length;
}

void FileWrapper::Close() {
  if (file_ != nullptr)
    fclose(file_);
  file_ = nullptr;
}

}  // namespace webrtc
