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
#include "api/video/video_frame.h"

namespace webrtc {
namespace test {

class VideoCodecTester {
 public:
  virtual ~VideoCodecTester() = default;

  struct TestSettings {
    bool realtime_decoding = false;
    bool realtime_encoding = false;
  };

  class TestRawVideoSource {
   public:
    virtual ~TestRawVideoSource() = default;

    virtual absl::optional<VideoFrame> PullFrame() = 0;
    virtual VideoFrame GetFrame(uint32_t timestamp_rtp) = 0;
  };

  class TestCodedVideoSource {
   public:
    virtual ~TestCodedVideoSource() = default;

    virtual absl::optional<EncodedImage> PullFrame() = 0;
  };

  class TestEncoder {
   public:
    virtual ~TestEncoder() = default;

    typedef absl::AnyInvocable<void(const EncodedImage& encoded_frame)>
        EncodeCallback;

    virtual void Encode(const VideoFrame& frame, EncodeCallback callback) = 0;
  };

  class TestDecoder {
   public:
    virtual ~TestDecoder() = default;

    typedef absl::AnyInvocable<void(const VideoFrame& decoded_frame)>
        DecodeCallback;

    virtual void Decode(const EncodedImage& frame, DecodeCallback callback) = 0;
  };

  virtual std::unique_ptr<VideoCodecTestStats> RunDecodeTest(
      std::unique_ptr<TestCodedVideoSource> video_source,
      const TestSettings& test_settings,
      std::unique_ptr<TestDecoder> decoder) = 0;

  virtual std::unique_ptr<VideoCodecTestStats> RunEncodeTest(
      std::unique_ptr<TestRawVideoSource> video_source,
      const TestSettings& test_settings,
      std::unique_ptr<TestEncoder> encoder) = 0;

  virtual std::unique_ptr<VideoCodecTestStats> RunEncodeDecodeTest(
      std::unique_ptr<TestRawVideoSource> video_source,
      const TestSettings& test_settings,
      std::unique_ptr<TestEncoder> encoder,
      std::unique_ptr<TestDecoder> decoder) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_TESTER_H_
