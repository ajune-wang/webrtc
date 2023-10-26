/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEO_CODEC_STATS_H_
#define API_TEST_VIDEO_CODEC_STATS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/metric.h"
#include "api/test/metrics/metrics_logger.h"
#include "api/test/video_codec_tester.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/video_codecs/scalability_mode.h"

namespace webrtc {
namespace test {

// Interface for encoded and/or decoded video frame and stream statistics.
class VideoCodecStats {
 public:
  using EncodingSettings = VideoCodecTester::EncodingSettings;
  using LayerId = VideoCodecTester::EncodingSettings::LayerId;

  // Filter for slicing frames.
  struct Filter {
    absl::optional<int> first_frame;
    absl::optional<int> last_frame;
    absl::optional<LayerId> layer_id;
  };

  struct Frame {
    int frame_num = 0;
    uint32_t timestamp_rtp = 0;

    int spatial_idx = 0;
    int temporal_idx = 0;

    std::vector<DecodeTargetIndication> decode_target_indications;

    int width = 0;
    int height = 0;
    DataSize frame_size = DataSize::Zero();
    bool keyframe = false;
    absl::optional<int> qp;

    Timestamp encode_start = Timestamp::Zero();
    TimeDelta encode_time = TimeDelta::Zero();
    Timestamp decode_start = Timestamp::Zero();
    TimeDelta decode_time = TimeDelta::Zero();

    struct Psnr {
      double y = 0.0;
      double u = 0.0;
      double v = 0.0;
    };
    absl::optional<Psnr> psnr;

    absl::optional<DataRate> target_bitrate;
    absl::optional<Frequency> target_framerate;

    bool encoded = false;
    bool decoded = false;

    absl::optional<EncodingSettings> encoding_settings;
  };

  struct Stream {
    SamplesStatsCounter width;
    SamplesStatsCounter height;
    SamplesStatsCounter frame_size_bytes;
    SamplesStatsCounter keyframe;
    SamplesStatsCounter qp;

    SamplesStatsCounter encode_time_ms;
    SamplesStatsCounter decode_time_ms;

    SamplesStatsCounter target_bitrate_kbps;
    SamplesStatsCounter target_framerate_fps;

    SamplesStatsCounter encoded_bitrate_kbps;
    SamplesStatsCounter encoded_framerate_fps;

    SamplesStatsCounter bitrate_mismatch_pct;
    SamplesStatsCounter framerate_mismatch_pct;

    SamplesStatsCounter transmission_time_ms;

    struct Psnr {
      SamplesStatsCounter y;
      SamplesStatsCounter u;
      SamplesStatsCounter v;
    } psnr;

    // Logs `Stream` metrics to provided `MetricsLogger`.
    void LogMetrics(MetricsLogger* logger,
                    std::string test_case_name,
                    std::map<std::string, std::string> metadata = {}) const;
  };

  virtual ~VideoCodecStats() = default;

  // Returns frames for the slice specified by `filter`. If `merge` is true,
  // also merge frames belonging to the same temporal unit into one superframe.
  virtual std::vector<Frame> Slice(Filter filter = Filter{},
                                   bool merge = false) const = 0;

  // Returns video statistics aggregated for the slice specified by `filter`.
  virtual Stream Aggregate(Filter filter = Filter{}) const = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_STATS_H_
