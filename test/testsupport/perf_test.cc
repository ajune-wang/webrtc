/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A stripped-down version of Chromium's chrome/test/perf/perf_test.cc.
// ResultsToString(), PrintResult(size_t value) and AppendResult(size_t value)
// have been modified. The remainder are identical to the Chromium version.

#include "api/optional.h"
#include "test/testsupport/perf_test.h"
#include "rtc_base/criticalsection.h"

#include <map>
#include <stdio.h>
#include <sstream>

namespace {

void PrintResultsImpl(const std::string& graph_name,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& units,
                      bool important) {
  // <*>RESULT <graph_name>: <trace_name>= <value> <units>
  // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>

  if (important) {
    printf("*");
  }
  printf("RESULT %s: %s= %s %s\n", graph_name.c_str(), trace.c_str(),
         values.c_str(), units.c_str());
}

class PerfValue {
 public:
  PerfValue(const double scalar) : scalar_(scalar) {}
  PerfValue(const double mean, const double error)
      : mean_and_error_({mean, error}) {}
  PerfValue(const std::vector<double>& list_of_scalars)
      : list_of_scalars_(list_of_scalars) {}
  std::string ToJSON() const;

 private:
  rtc::Optional<double> scalar_;
  rtc::Optional<std::pair<double, double>> mean_and_error_;
  rtc::Optional<std::vector<double>> list_of_scalars_;
};

std::string PerfValue::ToJSON() const {
  std::ostringstream value;
  if (scalar_.has_value()) {
    value << R"("type":"scalar",)";
    value << R"("value":)" << scalar_.value();
  } else if (mean_and_error_.has_value()) {
    value << R"("type":"scalar",)";
    value << R"("value":)" << mean_and_error_.value().first;
  } else {
    std::ostringstream value_stream;
    value_stream << '[';
    if (!list_of_scalars_.value().empty()) {
      auto it = list_of_scalars_->begin();
      while (true) {
        value_stream << *it;
        if (++it == list_of_scalars_->end())
          break;
        value_stream << ',';
      }
    }
    value_stream << ']';
    value << R"("type":"list_of_scalars",)";
    value << R"("values":)" << value_stream.str();
  }
  return value.str();
}

class PerfResult {
 public:
  // Constructor for scalar values
  PerfResult(const std::string& trace_name,
             const double value,
             const std::string& units)
      : trace_name_(trace_name),
        value_(value),
        units_(units) {}
  // Constructor for mean and error.
  PerfResult(const std::string& trace_name,
             const double mean,
             const double error,
             const std::string& units)
      : trace_name_(trace_name),
        value_(mean, error),
        units_(units) {}
  // Constructor for list of scalar values
  PerfResult(const std::string& trace_name,
             const std::vector<double>& list_of_scalars,
             const std::string& units)
      : trace_name_(trace_name),
        value_(list_of_scalars),
        units_(units) {}
  std::string ToJSON() const;

 private:
  std::string trace_name_;
  PerfValue value_;
  std::string units_;
};

std::string PerfResult::ToJSON() const {
  std::ostringstream result;
  result << "\"" << trace_name_ << "\":{";
  result << value_.ToJSON() << ',';
  result << R"("units":")" << units_ << "\",";
  result << R"("improvement_direction":undefined)";
  result << '}';
  return result.str();
}


class PerfResultsLogger {
 public:
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important) {
    std::ostringstream value_stream;
    value_stream << value;
    PrintResultsImpl(graph_name, trace_name, value_stream.str(), units,
                     important);

    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(PerfResult(trace_name, value, units));
  }
  void LogResultMeanAndError(const std::string& graph_name,
                             const std::string& trace_name,
                             const double mean,
                             const double error,
                             const std::string& units,
                             const bool important) {
    std::ostringstream value_stream;
    value_stream << '{' << mean << ',' << error << '}';
    PrintResultsImpl(graph_name, trace_name, value_stream.str(), units,
                     important);

    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(PerfResult(trace_name, mean, error, units));
  }
  void LogResultList(const std::string& graph_name,
                     const std::string& trace_name,
                     const std::vector<double> values,
                     const std::string& units,
                     const bool important) {
    std::ostringstream value_stream;
    value_stream << '[';
    if (!values.empty()) {
      auto it = values.begin();
      while (true) {
        value_stream << *it;
        if (++it == values.end())
          break;
        value_stream << ',';
      }
    }
    value_stream << ']';
    PrintResultsImpl(graph_name, trace_name, value_stream.str(), units,
                     important);

    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(PerfResult(trace_name, values, units));
  }
  std::string ToJSON() const;
 private:
  rtc::CriticalSection crit_;
  std::map<std::string, std::vector<PerfResult>> graphs_ RTC_GUARDED_BY(&crit_);
};

std::string PerfResultsLogger::ToJSON() const {
  std::ostringstream chart_data;
  chart_data << R"({"format_version":"1.0",)";
  chart_data << R"("benchmark_name":"webrtc_perf_tests",)";
  chart_data << R"("charts":{)";
  rtc::CritScope lock(&crit_);
  auto graphs_it = graphs_.begin();
  while (true) {
    chart_data << '"' << graphs_it->first << "\":{";
    auto traces_it = graphs_it->second.begin();
    while (true) {
      chart_data << traces_it->ToJSON();
      if (++traces_it == graphs_it->second.end())
        break;
      chart_data << ',';
    }
    chart_data << '}';
    if (++graphs_it == graphs_.end())
      break;
    chart_data << ',';
  }
  chart_data << "}}";
  return chart_data.str();
}

PerfResultsLogger& GetPerfResultsLogger() {
  static PerfResultsLogger* const logger_ = new PerfResultsLogger();
  return *logger_;
}

}  // namespace

namespace webrtc {
namespace test {

std::string GetPerfResultsJSON() {
  return GetPerfResultsLogger().ToJSON();
}

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const double value,
                 const std::string& units,
                 bool important) {
  GetPerfResultsLogger().LogResult(measurement + modifier, trace, value, units,
                                   important);
}

void PrintResultMeanAndError(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const double mean,
                             const double error,
                             const std::string& units,
                             bool important) {
  GetPerfResultsLogger().LogResultMeanAndError(
      measurement + modifier, trace, mean, error, units, important);
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const std::vector<double>& values,
                     const std::string& units,
                     bool important) {
  GetPerfResultsLogger().LogResultList(measurement + modifier, trace, values,
                                       units, important);
}

}  // namespace test
}  // namespace webrtc
