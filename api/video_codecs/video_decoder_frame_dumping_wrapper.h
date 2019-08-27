/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_DECODER_FRAME_DUMPING_WRAPPER_H_
#define API_VIDEO_CODECS_VIDEO_DECODER_FRAME_DUMPING_WRAPPER_H_

#include <memory>

#include "api/output_stream.h"
#include "api/video_codecs/video_decoder.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Wrap external VideoDecoders to provide a decoder that dumps frames in
// IVF format to the supplied RewindableOutputStream prior to decode.
RTC_EXPORT std::unique_ptr<VideoDecoder> CreateFrameDumpingDecoderWrapper(
    std::unique_ptr<VideoDecoder> wrapped_decoder,
    std::unique_ptr<RewindableOutputStream> output_stream);

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_DECODER_FRAME_DUMPING_WRAPPER_H_
