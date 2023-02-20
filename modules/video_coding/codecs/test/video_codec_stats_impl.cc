/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_stats_impl.h"

#include <algorithm>

#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace test {
namespace {
using Frame = VideoCodecStats::Frame;
using Stream = VideoCodecStats::Stream;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

class DataBuffer {
 public:
  explicit DataBuffer(int initial_buffer_level_bits)
      : buffer_level_bits_(initial_buffer_level_bits) {}

  DataBuffer() : DataBuffer(0) {}

  int LeakThenFill(int bits, Timestamp time, DataRate rate) {
    if (prev_time_) {
      // Leak with the previous data rate.
      TimeDelta passed = time - *prev_time_;
      buffer_level_bits_ -= prev_rate_->bps() * passed.us() / 1000;
      buffer_level_bits_ = std::max<int64_t>(buffer_level_bits_, 0);
    }

    buffer_level_bits_ += bits * 1000;

    prev_time_ = time;
    prev_rate_ = rate;

    return buffer_level_bits_ / 1000;
  }

 private:
  absl::optional<Timestamp> prev_time_;
  absl::optional<DataRate> prev_rate_;
  int64_t buffer_level_bits_;
};

Timestamp RtpToTime(uint32_t timestamp_rtp) {
  return Timestamp::Micros((timestamp_rtp / k90kHz).us());
}

SamplesStatsCounter::StatsSample StatsSample(double value, Timestamp time) {
  return SamplesStatsCounter::StatsSample{value, time};
}

void CalcRateMismatch(rtc::ArrayView<Frame> frames, Stream& stream) {
  int total_frame_size_bytes = 0;
  int num_encoded_frames = 0;
  for (const Frame& f : frames) {
    total_frame_size_bytes += f.frame_size_bytes;
    num_encoded_frames += f.encoded ? 1 : 0;
  }

  Timestamp first_time = RtpToTime(frames.begin()->timestamp_rtp);
  Timestamp last_time = RtpToTime(frames.rbegin()->timestamp_rtp);
  TimeDelta frame_duration = TimeDelta::Millis(
      rtc::kNumMillisecsPerSec / frames[0].target_framerate->millihertz());
  TimeDelta total_frame_duration = last_time - first_time + frame_duration;

  DataRate encoded_bitrate = DataRate::BitsPerSec(8 * total_frame_size_bytes *
                                                  rtc::kNumMillisecsPerSec /
                                                  total_frame_duration.ms());
  Frequency encoded_framerate =
      Frequency::MilliHertz(num_encoded_frames * rtc::kNumMicrosecsPerSec /
                            total_frame_duration.ms());

  double bitrate_mismatch_pct =
      100.0 * (encoded_bitrate.bps() - frames[0].target_bitrate->bps()) /
      frames[0].target_bitrate->bps();

  double framerate_mismatch_pct = 100.0 *
                                  (encoded_framerate.millihertz() -
                                   frames[0].target_framerate->millihertz()) /
                                  frames[0].target_framerate->millihertz();

  for (const Frame& f : frames) {
    Timestamp time = RtpToTime(f.timestamp_rtp);

    stream.encoded_bitrate_kbps.AddSample(
        StatsSample(encoded_bitrate.bps() / 1000.0, time));

    stream.encoded_framerate_fps.AddSample(
        StatsSample(encoded_framerate.millihertz() / 1000.0, time));

    stream.bitrate_mismatch_pct.AddSample(
        StatsSample(bitrate_mismatch_pct, time));

    stream.framerate_mismatch_pct.AddSample(
        StatsSample(framerate_mismatch_pct, time));
  }
}
}  // namespace

std::vector<Frame> VideoCodecStatsImpl::Slice(
    absl::optional<Filter> filter) const {
  std::vector<Frame> frames;
  for (const auto& [frame_id, f] : frames_) {
    if (filter.has_value()) {
      if (filter->first_frame.has_value() &&
          f.frame_num < *filter->first_frame) {
        continue;
      }
      if (filter->last_frame.has_value() && f.frame_num > *filter->last_frame) {
        continue;
      }
      if (filter->spatial_idx.has_value() &&
          f.spatial_idx != *filter->spatial_idx) {
        continue;
      }
      if (filter->temporal_idx.has_value() &&
          f.temporal_idx > *filter->temporal_idx) {
        continue;
      }
    }
    frames.push_back(f);
  }
  return frames;
}

Stream VideoCodecStatsImpl::Aggregate(const std::vector<Frame>& frames) const {
  std::vector<Frame> superframes = Merge(frames);
  DataBuffer data_buffer;
  size_t first_frame = 0;

  Stream stream;
  for (size_t i = 0; i < superframes.size(); ++i) {
    Frame& f = superframes[i];
    Timestamp time = Timestamp::Micros((f.timestamp_rtp / k90kHz).us());

    if (f.frame_size_bytes > 0) {
      stream.width.AddSample(StatsSample(f.width, time));
      stream.height.AddSample(StatsSample(f.height, time));
      stream.frame_size_bytes.AddSample(StatsSample(f.frame_size_bytes, time));
      stream.keyframe.AddSample(StatsSample(f.keyframe, time));
      if (f.qp) {
        stream.qp.AddSample(StatsSample(*f.qp, time));
      }
    }

    stream.encoded.AddSample(StatsSample(f.encoded, time));
    if (f.encoded) {
      stream.encode_time_ms.AddSample(StatsSample(f.encode_time.ms(), time));
    }

    stream.decoded.AddSample(StatsSample(f.decoded, time));
    if (f.decoded) {
      stream.decode_time_ms.AddSample(StatsSample(f.decode_time.ms(), time));
    }

    if (f.psnr) {
      stream.psnr.y.AddSample(StatsSample(f.psnr->y, time));
      stream.psnr.u.AddSample(StatsSample(f.psnr->u, time));
      stream.psnr.v.AddSample(StatsSample(f.psnr->v, time));
    }

    if (f.target_bitrate) {
      stream.target_bitrate_kbps.AddSample(
          StatsSample(f.target_bitrate->bps() / 1000.0, time));

      int buffer_level_bits = data_buffer.LeakThenFill(f.frame_size_bytes * 8,
                                                       time, *f.target_bitrate);
      int transmission_time_ms = buffer_level_bits * rtc::kNumMillisecsPerSec /
                                 f.target_bitrate->bps();
      stream.transmission_time_ms.AddSample(
          StatsSample(transmission_time_ms, time));
    }

    if (f.target_framerate) {
      stream.target_framerate_fps.AddSample(
          StatsSample(f.target_framerate->millihertz() / 1000.0, time));
    }

    bool rates_set =
        f.target_bitrate && superframes[first_frame].target_bitrate &&
        f.target_framerate && superframes[first_frame].target_framerate;
    bool eos = i + 1 == superframes.size();
    if (rates_set &&
        (eos || *f.target_bitrate != *superframes[first_frame].target_bitrate ||
         *f.target_framerate != *superframes[first_frame].target_framerate)) {
      size_t last_frame = i > 0 ? i - 1 : 0;
      CalcRateMismatch(rtc::ArrayView<Frame>(superframes.data() + first_frame,
                                             last_frame - first_frame + 1),
                       stream);
      first_frame = i;
    }
  }

  return stream;
}

void VideoCodecStatsImpl::LogMetrics(MetricsLogger* logger,
                                     const Stream& stream,
                                     std::string test_case_name) const {
  logger->LogMetric("encoded", test_case_name, stream.encoded, Unit::kCount,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("decoded", test_case_name, stream.decoded, Unit::kCount,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("width", test_case_name, stream.width, Unit::kCount,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("height", test_case_name, stream.height, Unit::kCount,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("frame_size_bytes", test_case_name, stream.frame_size_bytes,
                    Unit::kBytes,
                    webrtc::test::ImprovementDirection::kNeitherIsBetter);

  logger->LogMetric("keyframe", test_case_name, stream.keyframe, Unit::kCount,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter);

  logger->LogMetric("qp", test_case_name, stream.qp, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter);

  logger->LogMetric("encode_time_ms", test_case_name, stream.encode_time_ms,
                    Unit::kMilliseconds,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter);

  logger->LogMetric("decode_time_ms", test_case_name, stream.decode_time_ms,
                    Unit::kMilliseconds,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter);

  logger->LogMetric("target_bitrate_kbps", test_case_name,
                    stream.target_bitrate_kbps, Unit::kKilobitsPerSecond,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("target_framerate_fps", test_case_name,
                    stream.target_framerate_fps, Unit::kHertz,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("encoded_bitrate_kbps", test_case_name,
                    stream.encoded_bitrate_kbps, Unit::kKilobitsPerSecond,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("encoded_framerate_fps", test_case_name,
                    stream.encoded_framerate_fps, Unit::kHertz,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("bitrate_mismatch_pct", test_case_name,
                    stream.bitrate_mismatch_pct, Unit::kPercent,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter);

  logger->LogMetric("framerate_mismatch_pct", test_case_name,
                    stream.framerate_mismatch_pct, Unit::kPercent,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter);

  logger->LogMetric("transmission_time_ms", test_case_name,
                    stream.transmission_time_ms, Unit::kMilliseconds,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter);

  logger->LogMetric("psnr_y_db", test_case_name, stream.psnr.y, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("psnr_u_db", test_case_name, stream.psnr.u, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);

  logger->LogMetric("psnr_v_db", test_case_name, stream.psnr.v, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);
}

Frame* VideoCodecStatsImpl::AddFrame(int frame_num,
                                     uint32_t timestamp_rtp,
                                     int spatial_idx) {
  Frame frame;
  frame.frame_num = frame_num;
  frame.timestamp_rtp = timestamp_rtp;
  frame.spatial_idx = spatial_idx;

  FrameId frame_id;
  frame_id.frame_num = frame_num;
  frame_id.spatial_idx = spatial_idx;

  RTC_CHECK(frames_.find(frame_id) == frames_.end())
      << "Frame with frame_num=" << frame_num
      << " and spatial_idx=" << spatial_idx << " already exists";

  frames_[frame_id] = frame;

  if (frame_num_.find(timestamp_rtp) == frame_num_.end()) {
    frame_num_[timestamp_rtp] = frame_num;
  }

  return &frames_[frame_id];
}

Frame* VideoCodecStatsImpl::GetFrame(uint32_t timestamp_rtp, int spatial_idx) {
  if (frame_num_.find(timestamp_rtp) == frame_num_.end()) {
    return nullptr;
  }

  FrameId frame_id;
  frame_id.frame_num = frame_num_[timestamp_rtp];
  frame_id.spatial_idx = spatial_idx;

  if (frames_.find(frame_id) == frames_.end()) {
    return nullptr;
  }

  return &frames_[frame_id];
}

std::vector<Frame> VideoCodecStatsImpl::Merge(
    const std::vector<Frame>& frames) const {
  std::vector<Frame> superframes;
  // Map from frame_num to index in `superframes` vector.
  std::map<int, int> index;

  for (const auto& f : frames) {
    if (index.find(f.frame_num) == index.end()) {
      index[f.frame_num] = static_cast<int>(superframes.size());
      superframes.push_back(f);
      continue;
    }

    Frame& sf = superframes[index[f.frame_num]];

    sf.width = std::max(sf.width, f.width);
    sf.height = std::max(sf.height, f.height);
    sf.frame_size_bytes += f.frame_size_bytes;
    sf.keyframe |= f.keyframe;

    sf.encode_time = std::max(sf.encode_time, f.encode_time);
    sf.decode_time = std::max(sf.decode_time, f.decode_time);

    if (f.spatial_idx > sf.spatial_idx) {
      if (f.qp) {
        sf.qp = f.qp;
      }
      if (f.psnr) {
        sf.psnr = f.psnr;
      }
    }

    if (f.encoded || f.decoded) {
      sf.spatial_idx = std::max(sf.spatial_idx, f.spatial_idx);
      sf.temporal_idx = std::max(sf.temporal_idx, f.temporal_idx);
    }

    sf.encoded |= f.encoded;
    sf.decoded |= f.decoded;
  }

  return superframes;
}

}  // namespace test
}  // namespace webrtc
