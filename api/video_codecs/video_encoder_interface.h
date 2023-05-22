/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_INTERFACE_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_INTERFACE_H_

#include <map>
#include <memory>
// #include <unordered_set>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/numerics/rational.h"

namespace webrtc {
class VideoEncoderInterface {
 public:
  virtual ~VideoEncoderInterface() = default;

  enum class RateControlMode { kCqp, kCbr };
  struct Capabilities {
    struct PredictionConstraints {
      int num_buffers;
      int max_references;
      int max_temporal_layers;

      struct SpatialConstraints {
        int max_layers;
        // should be an unordered_set, hashing though...
        std::vector<Rational> scaling_factors;
        bool shared_buffer_space;
      };
      absl::optional<SpatialConstraints> spatial_constraints;
    } prediction_constraints;

    struct InputConstraints {
      Resolution min;
      Resolution max;
      int pixel_alignment;
      std::vector<VideoFrameBuffer::Type> input_formats;
    } input_constraints;

    // should be an unordered_set, hashing though...
    std::vector<EncodingFormat> encoding_formats;

    struct BitrateControl {
      bool frame_dropping;
      std::vector<VideoEncoderInterface::RateControlMode> supported_modes;
    } rate_control;

    struct Performance {
      absl::optional<int> max_encoded_pixels_per_seconds;
      std::pair<int, int> min_max_effort_level;
    } performance;
  };

  struct EncoderSettings {
    Resolution max_encode_dimensions;
    EncodingFormat encoding_format;
    VideoEncoderInterface::RateControlMode rc_mode;
    int max_number_of_threads;
  };

  struct TemporalUnitSettings {
    VideoCodecMode content_hint = VideoCodecMode::kRealtimeVideo;
    int effort_level = 0;
    enum class FrameDroppingMode { kOff, kAnyLayer, kAllLayers };
    FrameDroppingMode frame_dropping_mode = FrameDroppingMode::kOff;
  };

  struct FrameEncodeSettings {
    struct Cbr {
      TimeDelta duration;
      DataRate target_bitrate;
    };

    struct Cqp {
      int target_qp;
    };

    absl::variant<Cqp, Cbr> rate_options;

    bool keyframe = false;
    int temporal_id = 0;
    int spatial_id = 0;
    Rational resolution_scale = {1, 1};
    // Should be unordered_set
    std::vector<int> reference_buffers;
    std::vector<int> update_buffers;
  };

  // Results from calling Encode. Called once for each configured frame.
  // TODO: What to do when encoder breaks? Promise one `kError` callback
  // and then no more? One `kError` per expected frame? Something else?
  struct DroppedFrame {
    enum class Status { kDropped, kError };
    Status reason;
    int spatial_id;
  };

  struct EncodedData {
    rtc::scoped_refptr<EncodedImageBufferInterface> bitstream_data;
    bool is_keyframe;
    int spatial_id;
    int encoded_qp;
    std::vector<int> referenced_buffers;
  };

  using EncodeResult = std::variant<DroppedFrame, EncodedData>;
  using EncodeResultCallback =
      absl::AnyInvocable<void(const EncodeResult& result)>;

  // If `settings` and `frame_settings` are valid, returns true.
  virtual bool Encode(rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer,
                      const TemporalUnitSettings& settings,
                      const std::vector<FrameEncodeSettings>& frame_settings,
                      EncodeResultCallback encode_result_callback) = 0;
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_INTERFACE_H_
