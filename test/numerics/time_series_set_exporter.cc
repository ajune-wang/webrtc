/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/numerics/time_series_set_exporter.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#if WEBRTC_ENABLE_PROTOBUF
#include "test/numerics/time_series.pb.h"
#endif
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {

namespace {

void MaybeSerialize(const std::string& name,
                    const TimeSeriesSetExporter::TimeSeriesMap& time_series_map,
                    const std::vector<std::string>& key_insert_order,
                    std::string* serialized_proto) {
#if WEBRTC_ENABLE_PROTOBUF
  proto::TimeSeriesSet time_series_set;
  *time_series_set.mutable_name() = name;
  for (const std::string& key : key_insert_order) {
    const auto it = time_series_map.find(key);
    RTC_CHECK(it != time_series_map.end());
    proto::TimeSeries* time_series = time_series_set.add_timeseries();
    *time_series->mutable_name() = key;
    for (const auto& sample : it->second.GetTimedSamples()) {
      time_series->add_timestamps_us(sample.time.us());
      time_series->add_values(sample.value);
      time_series->add_annotations(sample.annotation);
    }
  }
  time_series_set.SerializeToString(serialized_proto);
#else
  RTC_LOG(LS_WARNING) << "Unable to serialize to binary protobuf";
#endif
}

}  // namespace

TimeSeriesSetExporter::TimeSeriesSetExporter(absl::string_view name)
    : name_(name) {}
TimeSeriesSetExporter::~TimeSeriesSetExporter() = default;

void TimeSeriesSetExporter::AddSample(const std::string& time_series_name,
                                      Timestamp timestamp,
                                      double value) {
  StoreInsertOrder(time_series_name);
  time_series_map_[time_series_name].AddSample(
      {.value = value, .time = timestamp});
}

void TimeSeriesSetExporter::AddSample(const std::string& time_series_name,
                                      Timestamp timestamp,
                                      double value,
                                      const std::string& annotation) {
  StoreInsertOrder(time_series_name);
  time_series_map_[time_series_name].AddSample(
      {.value = value, .time = timestamp, .annotation = annotation});
}

bool TimeSeriesSetExporter::ExportToBinaryProtobuf(
    absl::string_view output_path) {
  std::string serialized_proto;
  MaybeSerialize(name_, time_series_map_, key_insert_order_, &serialized_proto);
  if (serialized_proto.empty()) {
    return false;
  }
  FileWrapper file = FileWrapper::OpenWriteOnly(output_path);
  if (!file.is_open()) {
    return false;
  }
  return file.Write(serialized_proto.c_str(), serialized_proto.size());
}

void TimeSeriesSetExporter::StoreInsertOrder(
    const std::string& time_series_name) {
  if (time_series_map_.find(time_series_name) == time_series_map_.end()) {
    key_insert_order_.push_back(time_series_name);
  }
}

}  // namespace webrtc
