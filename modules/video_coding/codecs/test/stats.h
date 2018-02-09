/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_

#include <map>
#include <string>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {
namespace test {

// Statistics for one processed frame.
struct FrameStatistic {
  FrameStatistic(size_t frame_number, size_t rtp_timestamp)
      : frame_number(frame_number), rtp_timestamp(rtp_timestamp) {}

  std::string ToString() const;

  size_t frame_number = 0;
  size_t rtp_timestamp = 0;

  // Encoding.
  int64_t encode_start_ns = 0;
  int encode_return_code = 0;
  bool encoding_successful = false;
  size_t encode_time_us = 0;
  size_t target_bitrate_kbps = 0;
  size_t encoded_frame_size_bytes = 0;
  webrtc::FrameType frame_type = kVideoFrameDelta;

  // Layering.
  size_t temporal_layer_idx = 0;
  size_t simulcast_svc_idx = 0;

  // H264 specific.
  size_t max_nalu_size_bytes = 0;

  // Decoding.
  int64_t decode_start_ns = 0;
  int decode_return_code = 0;
  bool decoding_successful = false;
  size_t decode_time_us = 0;
  size_t decoded_width = 0;
  size_t decoded_height = 0;

  // Quantization.
  int qp = -1;

  // Quality.
  float psnr = 0.0;
  float ssim = 0.0;
};

struct VideoStatistic {
  std::string ToString(std::string prefix) const;

  size_t width = 0;
  size_t height = 0;

  size_t length_bytes = 0;
  size_t bitrate_kbps = 0;
  float framerate_fps = 0;

  float encoding_speed_fps = 0.0f;
  float decoding_speed_fps = 0.0f;

  float avg_delay_sec = 0.0f;
  float max_key_frame_delay_sec = 0.0f;
  float max_delta_frame_delay_sec = 0.0f;
  float time_to_reach_target_bitrate_sec = 0.0f;

  float avg_qp = 0.0f;

  size_t num_encoded_frames = 0;
  size_t num_decoded_frames = 0;
  size_t num_key_frames = 0;
  size_t num_spatial_resizes = 0;
  size_t max_nalu_size_bytes = 0;

  float avg_psnr = 0.0f;
  float min_psnr = 0.0f;
  float avg_ssim = 0.0f;
  float min_ssim = 0.0f;
};

// Statistics for a sequence of processed frames. This class is not thread safe.
class Stats {
 public:
  Stats() = default;
  ~Stats() = default;

  // Creates a FrameStatistic for the next frame to be processed.
  FrameStatistic* AddFrame(size_t timestamp, size_t spatial_layer_idx);

  // Returns the FrameStatistic corresponding to |frame_number| or |timestamp|.
  FrameStatistic* GetFrame(size_t frame_number, size_t spatial_layer_idx);
  FrameStatistic* GetFrameWithTimestamp(size_t timestamp,
                                        size_t spatial_layer_idx);

  VideoStatistic SliceAndCalcVideoStatistic(size_t first_frame_num,
                                            size_t last_frame_num,
                                            size_t spatial_layer_idx,
                                            size_t temporal_layer_idx,
                                            size_t target_kbps,
                                            float input_fps,
                                            bool aggregate_spatial_layers);

  FrameStatistic AggregateFrameStatistic(size_t frame_num,
                                         size_t spatial_layer_idx);

  size_t Size(size_t spatial_layer_idx);

  void Clear();

 private:
  std::map<size_t, std::vector<FrameStatistic>> layer_idx_to_stats_;
  std::map<size_t, std::map<size_t, size_t>> rtp_timestamp_to_frame_num_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_STATS_H_
