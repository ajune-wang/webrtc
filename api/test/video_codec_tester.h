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

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "api/video/encoded_image.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {
namespace test {

class VideoCodecStats;

// Interface for a video codec tester. The interface provides minimalistic set
// of data structures that enables implementation of decode-only, encode-only
// and encode-decode tests.
class VideoCodecTester {
 public:
  struct VideoSourceSettings {
    std::string file_path;
    Resolution resolution;
    Frequency framerate;
  };

  struct DecoderSettings {
    absl::optional<std::string> decoder_input_base_path;
    absl::optional<std::string> decoder_output_base_path;
  };

  struct EncoderSettings {
    absl::optional<std::string> encoder_input_base_path;
    absl::optional<std::string> encoder_output_base_path;
  };

  struct EncodingSettings {
    SdpVideoFormat sdp_video_format;
    ScalabilityMode scalability_mode;

    struct LayerId {
      int spatial_idx;
      int temporal_idx;
      bool operator==(const LayerId& o) const {
        return spatial_idx == o.spatial_idx && temporal_idx == o.temporal_idx;
      }
      bool operator<(const LayerId& o) const {
        if (spatial_idx < o.spatial_idx)
          return true;
        if (spatial_idx == o.spatial_idx && temporal_idx < o.temporal_idx)
          return true;
        return false;
      }
    };

    struct LayerSettings {
      Resolution resolution;
      Frequency framerate;
      DataRate bitrate;
    };

    std::map<LayerId, LayerSettings> layer_settings;
  };

  using FrameSettings = std::map<uint32_t, EncodingSettings>;

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
      VideoDecoder* decoder,
      const DecoderSettings& decoder_settings) = 0;

  // Pulls raw video frames from `video_source` and passes them to `encoder`.
  // Returns `VideoCodecTestStats` object that contains collected per-frame
  // metrics.
  virtual std::unique_ptr<VideoCodecStats> RunEncodeTest(
      const VideoSourceSettings& source_settings,
      VideoEncoderFactory* encoder_factory,
      const EncoderSettings& encoder_settings,
      const FrameSettings& frame_settings) = 0;

  // Pulls raw video frames from `video_source`, passes them to `encoder` and
  // then passes encoded frames to `decoder`. Returns `VideoCodecTestStats`
  // object that contains collected per-frame metrics.
  virtual std::unique_ptr<VideoCodecStats> RunEncodeDecodeTest(
      const VideoSourceSettings& source_settings,
      VideoEncoderFactory* encoder_factory,
      VideoDecoderFactory* decoder_factory,
      const EncoderSettings& encoder_settings,
      const DecoderSettings& decoder_settings,
      const FrameSettings& frame_settings) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_TESTER_H_
