/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_INTERFACE_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_INTERFACE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/resolution.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/numerics/rational.h"

namespace webrtc {
class VideoEncoderFactoryInterface {
 public:
  virtual ~VideoEncoderFactoryInterface() = default;

  virtual std::string CodecName() const = 0;
  virtual std::map<std::string, std::string> CodecSpecifics() const = 0;

  virtual VideoEncoderInterface::Capabilities GetEncoderCapabilities()
      const = 0;
  virtual std::unique_ptr<VideoEncoderInterface> CreateEncoder(
      const VideoEncoderInterface::EncoderSettings& settings,
      const std::map<std::string, std::string>& encoder_specific_settings) = 0;
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_INTERFACE_H_
