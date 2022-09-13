/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/memory_stream.h"

#include <errno.h>
#include <string.h>

#include <algorithm>

#include "rtc_base/checks.h"

namespace rtc {

StreamState MemoryStream::GetState() const {
  return SS_OPEN;
}

StreamResult MemoryStream::Read(void* buffer,
                                size_t bytes,
                                size_t* bytes_read,
                                int* error) {
  if (seek_position_ >= buffer_.size()) {
    return SR_EOS;
  }
  size_t available = buffer_.size() - seek_position_;
  if (bytes > available) {
    // Read partial buffer
    bytes = available;
  }
  memcpy(buffer, &buffer_[seek_position_], bytes);
  seek_position_ += bytes;
  if (bytes_read) {
    *bytes_read = bytes;
  }
  return SR_SUCCESS;
}

StreamResult MemoryStream::Write(const void* buffer,
                                 size_t bytes,
                                 size_t* bytes_written,
                                 int* error) {
  size_t available = buffer_.size() - seek_position_;
  if (0 == available) {
    // Increase buffer size to the larger of:
    // a) new position rounded up to next 256 bytes
    // b) double the previous length
    size_t new_buffer_length =
        std::max(((seek_position_ + bytes) | 0xFF) + 1, buffer_.size() * 2);
    RTC_DCHECK(ReserveSize(new_buffer_length));
    RTC_DCHECK(buffer_.size() >= new_buffer_length);
    available = buffer_.size() - seek_position_;
  }

  if (bytes > available) {
    bytes = available;
  }
  memcpy(&buffer_[seek_position_], buffer, bytes);
  seek_position_ += bytes;
  if (bytes_written) {
    *bytes_written = bytes;
  }
  return SR_SUCCESS;
}

void MemoryStream::Close() {
  // nothing to do
}

bool MemoryStream::SetPosition(size_t position) {
  if (position > buffer_.size())
    return false;
  seek_position_ = position;
  return true;
}

bool MemoryStream::GetPosition(size_t* position) const {
  if (position)
    *position = seek_position_;
  return true;
}

void MemoryStream::Rewind() {
  seek_position_ = 0;
}

bool MemoryStream::GetSize(size_t* size) const {
  if (size)
    *size = buffer_.size();
  return true;
}

bool MemoryStream::ReserveSize(size_t size) {
  buffer_.SetSize(size);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

MemoryStream::MemoryStream() = default;

MemoryStream::~MemoryStream() = default;

void MemoryStream::SetData(const void* data, size_t length) {
  buffer_.SetSize(length);
  if (data != nullptr && length > 0) {
    memcpy(buffer_.data(), data, length);
  }
  seek_position_ = 0;
}

}  // namespace rtc
