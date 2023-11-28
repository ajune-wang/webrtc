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
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/video/resolution.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/numerics/rational.h"

// TODO(vector/sets): Several members should conceptually be sets, but that
//                    requires lt comparison or hash functions... eh...
namespace webrtc {

using FrameDroppingMode = VideoEncoderInterface::FrameDroppingMode;
using FrameType = VideoEncoderInterface::FrameType;
using RateControlMode = VideoEncoderInterface::RateControlMode;

class VideoEncoderFactoryInterface {
 public:
  struct Capabilities {
    struct PredictionConstraints {
      int num_buffers;
      int max_references;
      int max_temporal_layers;

      int max_spatial_layers;
      std::vector<Rational> scaling_factors;
      bool shared_buffer_space;

      std::vector<FrameType> supported_frame_types;
    } prediction_constraints;

    struct InputConstraints {
      Resolution min;
      Resolution max;
      int pixel_alignment;
      std::vector<VideoFrameBuffer::Type> input_formats;
    } input_constraints;

    std::vector<EncodingFormat> encoding_formats;

    struct BitrateControl {
      std::vector<FrameDroppingMode> frame_dropping_modes;
      std::pair<int, int> qp_range;
      std::vector<RateControlMode> rc_modes;
    } rate_control;

    struct Performance {
      absl::optional<int> max_encoded_pixels_per_seconds;
      std::pair<int, int> min_max_effort_level;
    } performance;
  };

  struct StaticEncoderSettings {
    Resolution max_encode_dimensions;
    EncodingFormat encoding_format;
    RateControlMode rc_mode;
    int max_number_of_threads;
  };

  virtual ~VideoEncoderFactoryInterface() = default;

  virtual std::string CodecName() const = 0;
  virtual std::map<std::string, std::string> CodecSpecifics() const = 0;

  virtual Capabilities GetEncoderCapabilities() const = 0;
  virtual std::unique_ptr<VideoEncoderInterface> CreateEncoder(
      const StaticEncoderSettings& settings,
      const std::map<std::string, std::string>& encoder_specific_settings) = 0;
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_INTERFACE_H_
