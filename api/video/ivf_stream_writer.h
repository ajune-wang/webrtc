/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_IVF_STREAM_WRITER_H_
#define API_VIDEO_IVF_STREAM_WRITER_H_

#include <memory>

#include "api/output_stream.h"
#include "api/video/encoded_image.h"

namespace webrtc {


class IvfStreamWriter {
 public:
  virtual ~IvfStreamWriter() = default;

  virtual void WriteEncodedFrame(const EncodedImage& encoded_image,
                                 VideoCodecType codec_type) = 0;
};

std::unique_ptr<IvfStreamWriter> CreateIvfStreamWriter(
    std::unique_ptr<RewindableOutputStream> stream);

}  // namespace webrtc

#endif  // API_VIDEO_IVF_STREAM_WRITER_H_
