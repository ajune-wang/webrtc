/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/ivf_stream_writer.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/video_coding/utility/ivf_file_writer.h"

namespace webrtc {
namespace {

constexpr int kMaxFileSize = 1 * 1000 * 1000 * 1000;

class IvfStreamWriterImpl : public IvfStreamWriter {
 public:
  explicit IvfStreamWriterImpl(std::unique_ptr<RewindableOutputStream> stream)
      : file_writer_(IvfFileWriter::Wrap(std::move(stream), kMaxFileSize)) {}

  void WriteEncodedFrame(const EncodedImage& encoded_image,
                         VideoCodecType codec_type) override {
    file_writer_->WriteFrame(encoded_image, codec_type);
  }

 private:
  std::unique_ptr<IvfFileWriter> file_writer_;
};

}  // namespace

std::unique_ptr<IvfStreamWriter> CreateIvfStreamWriter(
    std::unique_ptr<RewindableOutputStream> stream) {
  return absl::make_unique<IvfStreamWriterImpl>(std::move(stream));
}

}  // namespace webrtc
