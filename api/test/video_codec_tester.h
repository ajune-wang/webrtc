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

#include "absl/functional/any_invocable.h"
#include "api/test/videocodec_test_stats.h"
#include "api/video/encoded_image.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"

namespace webrtc {
namespace test {

// Interface for a video codec tester. The interface provides minimalistic set
// of data structures that enable implementation of decode-only, encode-only
// and encode-decode tests and collecting per-frame metrics.
class VideoCodecTester {
 public:
  // Pacing settings for codec input data.
  struct PacingSettings {
    enum PacingMode {
      kNoPacing,
      // Pace with the rate equal to the target frame rate.
      kRealTime,
      // Pace with the explicitly provided rate.
      kConstRate,
    };
    PacingMode mode = PacingMode::kNoPacing;
    // Pacing rate for `kConstRate` mode.
    Frequency rate = Frequency::Zero();
  };

  struct DecodeSettings {
    PacingSettings pacing;
  };

  struct EncodeSettings {
    PacingSettings pacing;
  };

  virtual ~VideoCodecTester() = default;

  // Interface for a raw video frames source.
  class RawVideoSource {
   public:
    virtual ~RawVideoSource() = default;

    // Returns next frame. Frame RTP timestamp must be set. If no more frames to
    // pull, returns `absl::nullopt`.
    virtual absl::optional<VideoFrame> PullFrame() = 0;

    // Returns early pulled frame with RTC timestamp equal to `timestamp_rtp`.
    virtual VideoFrame GetFrame(uint32_t timestamp_rtp,
                                Resolution resolution) = 0;
  };

  // Interface for a coded video frames source.
  class CodedVideoSource {
   public:
    virtual ~CodedVideoSource() = default;

    // Returns next frame. If no more frames to pull, returns `absl::nullopt`.
    virtual absl::optional<EncodedImage> PullFrame() = 0;
  };

  // Interface for a video encoder.
  class Encoder {
   public:
    using EncodeCallback =
        absl::AnyInvocable<void(const EncodedImage& encoded_frame)>;

    virtual ~Encoder() = default;

    virtual void Encode(const VideoFrame& frame, EncodeCallback callback) = 0;
  };

  // Interface for a video decoder.
  class Decoder {
   public:
    using DecodeCallback =
        absl::AnyInvocable<void(const VideoFrame& decoded_frame)>;

    virtual ~Decoder() = default;

    virtual void Decode(const EncodedImage& frame, DecodeCallback callback) = 0;
  };

  // Pulls coded video frames from `video_source` and passes them to `decoder`.
  // Returns `VideoCodecTestStats` object that contains collected per-frame
  // metrics.
  virtual std::unique_ptr<VideoCodecTestStats> RunDecodeTest(
      std::unique_ptr<CodedVideoSource> video_source,
      std::unique_ptr<Decoder> decoder,
      const DecodeSettings& decode_settings) = 0;

  // Pulls raw video frames from `video_source` and passes them to `encoder`.
  // Returns `VideoCodecTestStats` object that contains collected per-frame
  // metrics.
  virtual std::unique_ptr<VideoCodecTestStats> RunEncodeTest(
      std::unique_ptr<RawVideoSource> video_source,
      std::unique_ptr<Encoder> encoder,
      const EncodeSettings& encode_settings) = 0;

  // Pulls raw video frames from `video_source`, passes them to `encoder` and
  // then passes encoded frames to `decoder`. Returns `VideoCodecTestStats`
  // object that contains collected per-frame metrics.
  virtual std::unique_ptr<VideoCodecTestStats> RunEncodeDecodeTest(
      std::unique_ptr<RawVideoSource> video_source,
      std::unique_ptr<Encoder> encoder,
      std::unique_ptr<Decoder> decoder,
      const EncodeSettings& encode_settings,
      const DecodeSettings& decode_settings) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_TESTER_H_
