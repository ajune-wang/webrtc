/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_IVF_FILE_WRITER_H_
#define MODULES_VIDEO_CODING_UTILITY_IVF_FILE_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "api/output_stream.h"
#include "api/video/encoded_image.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

class IvfFileWriter {
 public:
  // Takes ownership of the file, which will be closed on ~IvfFileWriter. If
  // writing a frame would take the file above the |byte_limit| the file will be
  // closed, the write (and all future writes) will fail. A |byte_limit| of 0 is
  // equivalent to no limit.
  static std::unique_ptr<IvfFileWriter> Wrap(FileWrapper file,
                                             size_t byte_limit);

  // Wraps an output stream which will be used to pass data in IVF file format.
  // The caller needs to guarantee that the |output_stream| object exists
  // through the IvfFileWriter's existence.
  static std::unique_ptr<IvfFileWriter> Wrap(
      std::unique_ptr<RewindableOutputStream> output_stream,
      size_t byte_limit);

  ~IvfFileWriter();

  bool WriteFrame(const EncodedImage& encoded_image, VideoCodecType codec_type);

 private:
  explicit IvfFileWriter(std::unique_ptr<RewindableOutputStream> output_stream,
                         size_t byte_limit);
  void Close();
  bool WriteHeader();
  bool InitFromFirstFrame(const EncodedImage& encoded_image,
                          VideoCodecType codec_type);

  VideoCodecType codec_type_;
  size_t bytes_written_;
  size_t byte_limit_;
  size_t num_frames_;
  uint16_t width_;
  uint16_t height_;
  int64_t last_timestamp_;
  bool using_capture_timestamps_;
  rtc::TimestampWrapAroundHandler wrap_handler_;
  std::unique_ptr<RewindableOutputStream> output_stream_;

  RTC_DISALLOW_COPY_AND_ASSIGN(IvfFileWriter);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_IVF_FILE_WRITER_H_
