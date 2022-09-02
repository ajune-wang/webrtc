/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NUMERICS_TIME_SERIES_SET_EXPORTER_H_
#define API_NUMERICS_TIME_SERIES_SET_EXPORTER_H_

#include <memory>
#include <string>

#include "api/units/timestamp.h"

namespace webrtc {

// The `TimeSeriesSetExporter` is a helper class that provides a simple way
// to dump time series data into a serialized proto file.
class TimeSeriesSetExporter {
 public:
  TimeSeriesSetExporter() = default;
  virtual ~TimeSeriesSetExporter() = default;

  // Adds a sample into the `time_series_name` time series. If no such time
  // series already exists, a new one is created.
  virtual void AddSample(const std::string& time_series_name,
                         Timestamp timestamp,
                         double value) = 0;

  // Adds a sample with a corresponding annotation.
  virtual void AddSample(const std::string& time_series_name,
                         Timestamp timestamp,
                         double value,
                         const std::string& annotation) = 0;

  // Exports all time series as a single `TimeSeriesSet` serialized proto file.
  // Returns true if file successfully written.
  virtual bool ExportToBinaryProtobuf(absl::string_view output_path) = 0;
};

}  // namespace webrtc

#endif  // API_NUMERICS_TIME_SERIES_SET_EXPORTER_H_
