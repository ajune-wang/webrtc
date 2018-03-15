/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_DECODER_H_
#define API_VIDEO_VIDEO_STREAM_DECODER_H_

#include <map>
#include <memory>
#include <utility>

#include "api/video/encoded_frame.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"

namespace webrtc {
// TODO(philipel): #include instead of forward declare when the relevant CL has
//                 landed.
class FrameKey;

// NOTE: This class is still under development and may change without notice.
class VideoStreamDecoder {
 public:
  struct DecodedFrameInfo {
    VideoFrame decodedImage;
    rtc::Optional<int32_t> decode_time_ms;
    rtc::Optional<uint8_t> qp;
  };

  static std::unique_ptr<VideoStreamDecoder> Create(
      std::unique_ptr<VideoDecoderFactory> decoder_factory,
      std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings,
      std::function<void()> non_decodable_callback,
      std::function<void(FrameKey)> continuous_callback,
      std::function<void(DecodedFrameInfo)> decoded_callback);

  virtual ~VideoStreamDecoder() = default;

  virtual void OnFrame(std::unique_ptr<video_coding::EncodedFrame> frame) = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_STREAM_DECODER_H_
