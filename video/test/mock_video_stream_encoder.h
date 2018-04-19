/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_H_
#define VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_H_

#include "test/gmock.h"
#include "video/video_stream_encoder.h"

namespace webrtc {

class MockVideoStreamEncoder : public VideoStreamEncoderInterface {
 public:
  MOCK_METHOD0(SendKeyFrame, void());
  MOCK_METHOD1(SetStartBitrate, void(int));
  MOCK_METHOD2(SetSink, void(EncoderSink*, bool));
  MOCK_METHOD3(OnBitrateUpdated, void(uint32_t, uint8_t, int64_t));
  MOCK_METHOD1(OnFrame, void(const VideoFrame&));
};

}  // namespace webrtc

#endif  // VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_H_
