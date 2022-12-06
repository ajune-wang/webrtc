/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TESTER_IMPL_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TESTER_IMPL_H_

#include <memory>

#include "api/test/video_codec_tester.h"

namespace webrtc {
namespace test {

class VideoCodecTesterImpl : public VideoCodecTester {
 public:
  std::unique_ptr<VideoCodecTestStats> RunDecodeTest(
      std::unique_ptr<CodedVideoSource> video_source,
      std::unique_ptr<Decoder> decoder,
      const DecodeSettings& decode_settings) override;

  std::unique_ptr<VideoCodecTestStats> RunEncodeTest(
      std::unique_ptr<RawVideoSource> video_source,
      std::unique_ptr<Encoder> encoder,
      const EncodeSettings& encode_settings) override;

  std::unique_ptr<VideoCodecTestStats> RunEncodeDecodeTest(
      std::unique_ptr<RawVideoSource> video_source,
      std::unique_ptr<Encoder> encoder,
      std::unique_ptr<Decoder> decoder,
      const EncodeSettings& encode_settings,
      const DecodeSettings& decode_settings) override;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TESTER_IMPL_H_
