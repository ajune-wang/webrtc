/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NUMERICS_TIME_SERIES_SET_EXPORTER_H_
#define TEST_NUMERICS_TIME_SERIES_SET_EXPORTER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/units/timestamp.h"

namespace webrtc {

// The `TimeSeriesSetExporter` is a helper class that provides a simple way
// to dump time series data into a serialized proto file.
class TimeSeriesSetExporter {
 public:
  using TimeSeriesMap = std::unordered_map<std::string, SamplesStatsCounter>;

  explicit TimeSeriesSetExporter(absl::string_view name);
  TimeSeriesSetExporter(const TimeSeriesSetExporter&) = delete;
  TimeSeriesSetExporter& operator=(const TimeSeriesSetExporter&) = delete;
  ~TimeSeriesSetExporter();

  // Adds a sample into the `time_series_name` time series. If no such time
  // series already exists, a new one is created.
  void AddSample(const std::string& time_series_name,
                 Timestamp timestamp,
                 double value);

  // Adds a sample with a corresponding annotation.
  void AddSample(const std::string& time_series_name,
                 Timestamp timestamp,
                 double value,
                 const std::string& annotation);

  // Exports all time series as a single `TimeSeriesSet` serialized proto file.
  // Returns true if file successfully written.
  bool ExportToBinaryProtobuf(absl::string_view output_path);

 private:
  void StoreInsertOrder(const std::string& time_series_name);

  const std::string name_;
  std::vector<std::string> key_insert_order_;
  TimeSeriesMap time_series_map_;
};

}  // namespace webrtc

#endif  // TEST_NUMERICS_TIME_SERIES_SET_EXPORTER_H_
