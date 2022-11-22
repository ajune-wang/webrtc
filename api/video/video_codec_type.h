/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_CODEC_TYPE_H_
#define API_VIDEO_VIDEO_CODEC_TYPE_H_

#include <string>

namespace webrtc {

enum VideoCodecType {
  // There are various memset(..., 0, ...) calls in the code that rely on
  // kVideoCodecGeneric being zero.
  kVideoCodecGeneric = 0,
  kVideoCodecVP8,
  kVideoCodecVP9,
  kVideoCodecAV1,
  kVideoCodecH264,
  kVideoCodecMultiplex,
};

inline const char* VideoCodecTypeToString(VideoCodecType video_codec_type) {
  switch (video_codec_type) {
    case VideoCodecType::kVideoCodecGeneric:
      return "generic";
    case VideoCodecType::kVideoCodecVP8:
      return "VP8";
    case VideoCodecType::kVideoCodecVP9:
      return "VP9";
    case VideoCodecType::kVideoCodecAV1:
      return "AV1";
    case VideoCodecType::kVideoCodecH264:
      return "H264";
    case VideoCodecType::kVideoCodecMultiplex:
      return "Multiplex";
    default:
      return "UNKNOWN";
  }
}

inline bool VideoCodecTypeFromString(std::string str, VideoCodecType* ret) {
  if (str == "generic") {
    *ret = VideoCodecType::kVideoCodecGeneric;
    return true;
  }
  if (str == "VP8") {
    *ret = VideoCodecType::kVideoCodecVP8;
    return true;
  }
  if (str == "VP9") {
    *ret = VideoCodecType::kVideoCodecVP9;
    return true;
  }
  if (str == "AV1") {
    *ret = VideoCodecType::kVideoCodecAV1;
    return true;
  }
  if (str == "H264") {
    *ret = VideoCodecType::kVideoCodecH264;
    return true;
  }
  if (str == "Multiplex") {
    *ret = VideoCodecType::kVideoCodecMultiplex;
    return true;
  }
  return false;
}

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_CODEC_TYPE_H_
