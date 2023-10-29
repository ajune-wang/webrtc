/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEO_CODEC_TESTER_H_
#define API_TEST_VIDEO_CODEC_TESTER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/video_codec_stats.h"
#include "api/test/video_codec_test_common.h"
#include "api/units/frequency.h"
#include "api/video/encoded_image.h"
#include "api/video/resolution.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {
namespace test {

// Interface for a video codec tester. The interface provides minimalistic set
// of data structures that enables implementation of decode-only, encode-only
// and encode-decode tests.
class VideoCodecTester {
 public:
  // Pacing settings for codec input.
  struct PacingSettings {
    enum PacingMode {
      // Pacing is not used. Frames are sent to codec back-to-back.
      kNoPacing,
      // Pace with the rate equal to the target video frame rate. Pacing time is
      // derived from RTP timestamp.
      kRealTime,
      // Pace with the explicitly provided rate.
      kConstantRate,
    };
    PacingMode mode = PacingMode::kNoPacing;
    // Pacing rate for `kConstantRate` mode.
    Frequency constant_rate = Frequency::Zero();
  };

  struct VideoSourceSettings {
    std::string file_path;
    Resolution resolution;
    Frequency framerate;
  };

  struct DecoderSettings {
    PacingSettings pacing_settings;
    absl::optional<std::string> decoder_input_base_path;
    absl::optional<std::string> decoder_output_base_path;
  };

  struct EncoderSettings {
    PacingSettings pacing_settings;
    absl::optional<std::string> encoder_input_base_path;
    absl::optional<std::string> encoder_output_base_path;
  };

  virtual ~VideoCodecTester() = default;

  // Interface for a coded video frames source.
  class CodedVideoSource {
   public:
    virtual ~CodedVideoSource() = default;

    // Returns next frame. If no more frames to pull, returns `absl::nullopt`.
    // For analysis and pacing purposes, frame must have RTP timestamp set. The
    // timestamp must represent the target video frame rate and be unique.
    virtual absl::optional<EncodedImage> PullFrame() = 0;
  };

  // Pulls coded video frames from `video_source` and passes them to `decoder`.
  // Returns `VideoCodecTestStats` object that contains collected per-frame
  // metrics.
  virtual std::unique_ptr<VideoCodecStats> RunDecodeTest(
      CodedVideoSource* video_source,
      VideoDecoderFactory* decoder_factory,
      const DecoderSettings& decoder_settings,
      const SdpVideoFormat& sdp_video_format) = 0;

  // Pulls raw video frames from `video_source` and passes them to `encoder`.
  // Returns `VideoCodecTestStats` object that contains collected per-frame
  // metrics.
  virtual std::unique_ptr<VideoCodecStats> RunEncodeTest(
      const VideoSourceSettings& source_settings,
      VideoEncoderFactory* encoder_factory,
      const EncoderSettings& encoder_settings,
      const std::map<uint32_t, EncodingSettings>& encoding_settings) = 0;

  // Pulls raw video frames from `video_source`, passes them to `encoder` and
  // then passes encoded frames to `decoder`. Returns `VideoCodecTestStats`
  // object that contains collected per-frame metrics.
  virtual std::unique_ptr<VideoCodecStats> RunEncodeDecodeTest(
      const VideoSourceSettings& source_settings,
      VideoEncoderFactory* encoder_factory,
      VideoDecoderFactory* decoder_factory,
      const EncoderSettings& encoder_settings,
      const DecoderSettings& decoder_settings,
      const std::map<uint32_t, EncodingSettings>& encoding_settings) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_TESTER_H_
