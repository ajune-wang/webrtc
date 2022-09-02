/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_TIME_SERIES_SET_EXPORTER_IMPL_H_
#define RTC_BASE_NUMERICS_TIME_SERIES_SET_EXPORTER_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/numerics/time_series_set_exporter.h"
#include "api/units/timestamp.h"

namespace webrtc {

class TimeSeriesSetExporterImpl : public TimeSeriesSetExporter {
 public:
  using TimeSeriesMap = std::unordered_map<std::string, SamplesStatsCounter>;

  explicit TimeSeriesSetExporterImpl(absl::string_view name);
  ~TimeSeriesSetExporterImpl() override;

  // Implements `TimeSeriesSetExporter`.
  void AddSample(const std::string& time_series_name,
                 Timestamp timestamp,
                 double value) override;
  void AddSample(const std::string& time_series_name,
                 Timestamp timestamp,
                 double value,
                 const std::string& annotation) override;
  bool ExportToBinaryProtobuf(absl::string_view output_path) override;

 private:
  void StoreInsertOrder(const std::string& time_series_name);

  const std::string name_;
  std::vector<std::string> key_insert_order_;
  TimeSeriesMap time_series_map_;
};

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_TIME_SERIES_SET_EXPORTER_IMPL_H_
