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

class TimeSeriesSetExporter {
 public:
  TimeSeriesSetExporter() = default;
  virtual ~TimeSeriesSetExporter() = default;

  // Add a sample.
  virtual void AddSample(const std::string& time_series_name, double value) = 0;

  // Add a sample with a given timestamp.
  virtual void AddSample(const std::string& time_series_name,
                         Timestamp timestamp,
                         double value) = 0;

  // Export all time series with all values.
  virtual void ExportToBinaryPb(absl::string_view output_dir) = 0;
};

}  // namespace webrtc

#endif  // API_NUMERICS_TIME_SERIES_SET_EXPORTER_H_
