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
#include "api/test/video/video_frame_reader.h"
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

    int num_frames = 0;
  };

  struct FrameSettings {
    int bitrate_kbps = 0;
    int framerate_fps = 0;
  };

  class TestEncoder {
   public:
    virtual ~TestEncoder() = default;

    typedef absl::AnyInvocable<void(const EncodedImage& encoded_frame,
                                    const FrameSettings& frame_settings)>
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

  virtual std::unique_ptr<VideoCodecTestStats> RunEncodeDecodeTest(
      std::unique_ptr<VideoFrameReader> frame_reader,
      const TestSettings& test_settings,
      std::unique_ptr<TestEncoder> encoder,
      std::unique_ptr<TestDecoder> decoder) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_TESTER_H_
