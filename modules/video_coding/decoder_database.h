/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_DECODER_DATABASE_H_
#define MODULES_VIDEO_CODING_DECODER_DATABASE_H_

#include <stdint.h>

#include <map>

#include "absl/types/optional.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/encoded_frame.h"
#include "modules/video_coding/generic_decoder.h"

namespace webrtc {

class VCMDecoderDataBase {
 public:
  explicit VCMDecoderDataBase(VCMDecodedFrameCallback* callback);
  VCMDecoderDataBase(const VCMDecoderDataBase&) = delete;
  VCMDecoderDataBase& operator=(const VCMDecoderDataBase&) = delete;
  ~VCMDecoderDataBase() = default;

  bool DeregisterExternalDecoder(uint8_t payload_type);
  void RegisterExternalDecoder(uint8_t payload_type,
                               VideoDecoder* external_decoder);
  bool IsExternalDecoderRegistered(uint8_t payload_type) const;

  bool RegisterReceiveCodec(uint8_t payload_type,
                            const VideoCodec& receive_codec,
                            int number_of_cores);
  bool DeregisterReceiveCodec(uint8_t payload_type);

  // Decodes frame with decoder registered with `payload_type' equals to
  // `frame.payloadType()`
  // Reinitializes the decoder when that payload type changes.
  // As return value forwards error code returned by `VideoDecoder::Decode`.
  int32_t Decode(const VCMEncodedFrame& frame, Timestamp now);

 private:
  struct DecoderSettings {
    VideoCodec settings;
    int number_of_cores;
  };
  struct CurrentDecoderState {
    CurrentDecoderState(uint8_t payload_type, VideoDecoder* decoder);
    ~CurrentDecoderState();

    const uint8_t payload_type = 0;
    VideoDecoder* const decoder = nullptr;
    VideoContentType content_type = VideoContentType::UNSPECIFIED;
    VideoDecoder::DecoderInfo decoder_info;
  };

  // Sets current_ decoder specified by frame.PayloadType. The decoded frame
  // callback of the decoder is set to `callback_`. If no such decoder exists
  // current_ will be set to absl::nullopt.
  void PickDecoder(const VCMEncodedFrame& frame);

  VCMDecodedFrameCallback* const callback_;
  absl::optional<CurrentDecoderState> current_;
  // Initialization paramaters for decoders keyed by payload type.
  std::map<uint8_t, DecoderSettings> decoder_settings_;
  // Decoders keyed by payload type.
  std::map<uint8_t, VideoDecoder*> decoders_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_DECODER_DATABASE_H_
