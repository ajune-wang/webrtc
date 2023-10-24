/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/video_codec_stats.h"

#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace test {

namespace {
const std::map<DecodeTargetIndication, std::string>
    kDecodeTargetIndicationName = {{DecodeTargetIndication::kNotPresent, "-"},
                                   {DecodeTargetIndication::kDiscardable, "D"},
                                   {DecodeTargetIndication::kSwitch, "S"},
                                   {DecodeTargetIndication::kRequired, "R"}};

template <typename Range>
std::string StrJoin(const Range& seq, absl::string_view delimiter) {
  rtc::StringBuilder sb;
  int idx = 0;

  for (const typename Range::value_type& elem : seq) {
    if (idx > 0) {
      sb << delimiter;
    }
    sb << elem;

    ++idx;
  }
  return sb.Release();
}

}  // namespace

void VideoCodecStats::Stream::LogMetrics(
    MetricsLogger* logger,
    absl::string_view test_case_name,
    std::string metric_prefix,
    std::map<std::string, std::string> metadata) const {
  logger->LogMetric(
      metric_prefix + "width", test_case_name, width, Unit::kCount,
      webrtc::test::ImprovementDirection::kBiggerIsBetter, metadata);

  logger->LogMetric(
      metric_prefix + "height", test_case_name, height, Unit::kCount,
      webrtc::test::ImprovementDirection::kBiggerIsBetter, metadata);

  logger->LogMetric(metric_prefix + "frame_size_bytes", test_case_name,
                    frame_size_bytes, Unit::kBytes,
                    webrtc::test::ImprovementDirection::kNeitherIsBetter,
                    metadata);

  logger->LogMetric(
      metric_prefix + "keyframe", test_case_name, keyframe, Unit::kCount,
      webrtc::test::ImprovementDirection::kSmallerIsBetter, metadata);

  logger->LogMetric(metric_prefix + "qp", test_case_name, qp, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric(metric_prefix + "encode_time_ms", test_case_name,
                    encode_time_ms, Unit::kMilliseconds,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric(metric_prefix + "decode_time_ms", test_case_name,
                    decode_time_ms, Unit::kMilliseconds,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  // kKilobitsPerSecond is always converted to bytesPerSecond in
  // third_party/webrtc/api/test/metrics/print_result_proxy_metrics_exporter.cc.
  // Use kUnitless to avoid that conversion and keep it understandable.
  logger->LogMetric(metric_prefix + "target_bitrate_kbps", test_case_name,
                    target_bitrate_kbps, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric(metric_prefix + "target_framerate_fps", test_case_name,
                    target_framerate_fps, Unit::kHertz,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric(metric_prefix + "encoded_bitrate_kbps", test_case_name,
                    encoded_bitrate_kbps, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric(metric_prefix + "encoded_framerate_fps", test_case_name,
                    encoded_framerate_fps, Unit::kHertz,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric(metric_prefix + "bitrate_mismatch_pct", test_case_name,
                    bitrate_mismatch_pct, Unit::kPercent,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric(metric_prefix + "framerate_mismatch_pct", test_case_name,
                    framerate_mismatch_pct, Unit::kPercent,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric(metric_prefix + "transmission_time_ms", test_case_name,
                    transmission_time_ms, Unit::kMilliseconds,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric(
      metric_prefix + "psnr_y_db", test_case_name, psnr.y, Unit::kUnitless,
      webrtc::test::ImprovementDirection::kBiggerIsBetter, metadata);

  logger->LogMetric(
      metric_prefix + "psnr_u_db", test_case_name, psnr.u, Unit::kUnitless,
      webrtc::test::ImprovementDirection::kBiggerIsBetter, metadata);

  logger->LogMetric(
      metric_prefix + "psnr_v_db", test_case_name, psnr.v, Unit::kUnitless,
      webrtc::test::ImprovementDirection::kBiggerIsBetter, metadata);
}

std::map<std::string, std::string> VideoCodecStats::Frame::ToMap() const {
  std::map<std::string, std::string> map;
  map["frame_num"] = std::to_string(frame_num);
  map["timestamp_rtp"] = std::to_string(timestamp_rtp);
  map["spatial_idx"] = std::to_string(spatial_idx);
  map["temporal_idx"] = std::to_string(temporal_idx);
  map["width"] = std::to_string(width);
  map["height"] = std::to_string(height);
  map["frame_size_bytes"] = std::to_string(frame_size.bytes());
  map["keyframe"] = std::to_string(keyframe ? 1 : 0);
  map["qp"] = qp ? std::to_string(*qp) : "";
  map["encode_time_ms"] = std::to_string(encode_time.ms());
  map["decode_time_ms"] = std::to_string(decode_time.ms());
  map["psnr_y_db"] = psnr ? std::to_string(psnr->y) : "";
  map["psnr_u_db"] = psnr ? std::to_string(psnr->u) : "";
  map["psnr_v_db"] = psnr ? std::to_string(psnr->v) : "";
  map["target_bitrate_kbps"] =
      target_bitrate ? std::to_string(target_bitrate->kbps()) : "";
  map["target_framerate_fps"] =
      target_framerate ? std::to_string(target_framerate->hertz<double>()) : "";

  std::string decode_target_indications_str;
  for (const auto& dti : decode_target_indications) {
    decode_target_indications_str += kDecodeTargetIndicationName.at(dti);
  }
  map["decode_target_indications"] = decode_target_indications_str;
  return map;
}

void VideoCodecStats::LogMetrics(
    absl::string_view csv_file_path,
    std::vector<Frame> frames,
    std::map<std::string, std::string> metadata) const {
  FILE* csv_file = fopen(csv_file_path.data(), "w");
  const std::string delimiter = ";";

  std::vector<std::string> metric_names;
  for (const auto& metric : frames[0].ToMap()) {
    metric_names.push_back(metric.first);
  }

  for (const auto& metric : metadata) {
    metric_names.push_back(metric.first);
  }

  std::string header = StrJoin(metric_names, delimiter);
  fwrite(header.c_str(), 1, header.size(), csv_file);

  for (const auto& f : frames) {
    const auto& metrics = f.ToMap();
    std::vector<std::string> metric_values;
    std::transform(metrics.begin(), metrics.end(),
                   std::back_inserter(metric_values),
                   [](const auto& metric) { return metric.second; });

    for (const auto& metric : metadata) {
      metric_values.push_back(metric.second);
    }

    std::string row = StrJoin(metric_values, delimiter);
    fwrite("\n", 1, 1, csv_file);
    fwrite(row.c_str(), 1, row.size(), csv_file);
  }

  fclose(csv_file);
}

}  // namespace test
}  // namespace webrtc
