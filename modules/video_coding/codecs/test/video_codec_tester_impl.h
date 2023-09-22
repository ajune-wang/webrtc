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

// A stateless implementation of `VideoCodecTester`. This class is thread safe.
class VideoCodecTesterImpl : public VideoCodecTester {
 public:
  std::unique_ptr<VideoCodecStats> RunDecodeTest(
      CodedVideoSource* video_source,
    std::map<int, VideoDecoder*> decoders,
    const DecoderSettings& decoder_settings) override;

  std::unique_ptr<VideoCodecStats> RunEncodeTest(
      RawVideoSource* video_source,
      std::map<int, VideoEncoder*> encoders,
      const EncoderSettings& encoder_settings,
      const std::map<int, EncodingSettings>& frame_settings) override;

  std::unique_ptr<VideoCodecStats> RunEncodeDecodeTest(
    RawVideoSource* video_source,
    std::map<int, VideoEncoder*> encoders,
    std::map<int, VideoDecoder*> decoders,
    const EncoderSettings& encoder_settings,
    const DecoderSettings& decoder_settings,
    const std::map<int, EncodingSettings>& frame_settings) override;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TESTER_IMPL_H_
