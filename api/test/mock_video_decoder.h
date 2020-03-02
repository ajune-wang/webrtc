/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_VIDEO_DECODER_H_
#define API_TEST_MOCK_VIDEO_DECODER_H_

#include "api/video_codecs/video_decoder.h"
#include "test/gmock.h"

namespace webrtc {

class MockDecodedImageCallback : public DecodedImageCallback {
 public:
  MockDecodedImageCallback();
  ~MockDecodedImageCallback() override;

  MOCK_METHOD(int32_t, Decoded, (VideoFrame & decodedImage), (override));
  MOCK_METHOD(int32_t,
              Decoded,
              (VideoFrame & decodedImage, int64_t decode_time_ms),
              (override));
  MOCK_METHOD(void,
              Decoded,
              (VideoFrame & decodedImage,
               absl::optional<int32_t> decode_time_ms,
               absl::optional<uint8_t> qp),
              (override));
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder();
  ~MockVideoDecoder() override;

  MOCK_METHOD(int32_t,
              InitDecode,
              (const VideoCodec*, int32_t numberOfCores),
              (override));
  MOCK_METHOD(int32_t,
              Decode,
              (const EncodedImage& inputImage,
               bool missingFrames,
               int64_t renderTimeMs),
              (override));
  MOCK_METHOD(int32_t,
              RegisterDecodeCompleteCallback,
              (DecodedImageCallback*),
              (override));
  MOCK_METHOD(int32_t, Release, (), (override));
  MOCK_METHOD(VideoDecoder*, Copy, (), (override));
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_VIDEO_DECODER_H_
