/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_BLACK_FRAME_DECODER_H_
#define MODULES_VIDEO_CODING_BLACK_FRAME_DECODER_H_

#include "api/video/i420_buffer.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"

namespace webrtc {
// A video decoder class that is used for testing in Chrome.
// Does not decode but returns a black frame of the appropriate size
// if the encoded image width and height is set.
class BlackFrameDecoder : public webrtc::VideoDecoder {
 public:
  explicit BlackFrameDecoder(SdpVideoFormat format);
  bool Configure(const Settings& settings) override;
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  int32_t Release() override;

  const char* ImplementationName() const override;

 private:
  webrtc::VideoFrame CreateFrame(int width, int height, uint32_t timestamp);

  DecodedImageCallback* decode_complete_callback_;
  int width_;
  int height_;
  int qp_;
  VideoCodecType type_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_BLACK_FRAME_DECODER_H_
