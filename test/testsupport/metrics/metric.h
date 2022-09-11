/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_METRICS_METRIC_H_
#define TEST_TESTSUPPORT_METRICS_METRIC_H_

#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"

namespace webrtc {
namespace test {

enum class Unit {
  kTimeMs,
  kPercent,
  kSizeInBytes,
  kKilobitsPerSecond,
  kHertz,
  // General unitless value.
  kUnitless,
  // Count of some items.
  kCount
};
std::string ToString(Unit unit);

enum class ImprovementDirection {
  kBiggerIsBetter,
  kBothPossible,
  kSmallerIsBetter
};
std::string ToString(ImprovementDirection direction);

struct Metric {
  struct TimeSeries {
    struct DataPoint {
      // Timestamp in microseconds associated with a data point. May be the time
      // when the data point was collected.
      webrtc::Timestamp timestamp;
      double value;
      // Metadata associated with this particular data point.
      std::map<std::string, std::string> metadata;
    };

    // All values collected for this metric. May be omitted if only
    // overall stats were collected by the test.
    std::vector<DataPoint> values;
  };

  // Contains metric's precomputed statistics based on the `time_series` or if
  // `time_series` is omitted (has 0 values) contains precomputed statistics
  // provided by the metric's calculator.
  struct Stats {
    // Sample mean of the metric
    // (https://en.wikipedia.org/wiki/Sample_mean_and_covariance).
    absl::optional<double> mean;
    // Standard deviation (https://en.wikipedia.org/wiki/Standard_deviation).
    // Is undefined if `time_series` contains only a single value.
    absl::optional<double> stddev;
    absl::optional<double> min;
    absl::optional<double> max;
  };

  // Metric name, for example PSNR, SSIM, decode_time, etc.
  std::string name;
  Unit unit;
  ImprovementDirection improvement_direction;
  std::string test_case;
  // Metadata associated with the whole metric.
  std::map<std::string, std::string> metadata;
  // Contains all values of the metric collected during test execution.
  // Can be omitted if no particular values were provided, but only aggregated
  // statistics was computed. In this case only the `stats` object will be
  // presented.
  TimeSeries time_series;
  Stats stats;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_METRICS_METRIC_H_
