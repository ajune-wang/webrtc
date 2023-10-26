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
      VideoDecoderFactory* decoder_factory,
      const DecoderSettings& decoder_settings,
      const FramesSettings& frames_settings) override;

  std::unique_ptr<VideoCodecStats> RunEncodeTest(
      const VideoSourceSettings& source_settings,
      VideoEncoderFactory* encoder_factory,
      const EncoderSettings& encoder_settings,
      const FramesSettings& frame_settings) override;

  std::unique_ptr<VideoCodecStats> RunEncodeDecodeTest(
      const VideoSourceSettings& source_settings,
      VideoEncoderFactory* encoder_factory,
      VideoDecoderFactory* decoder_factory,
      const EncoderSettings& encoder_settings,
      const DecoderSettings& decoder_settings,
      const FramesSettings& frame_settings) override;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TESTER_IMPL_H_
