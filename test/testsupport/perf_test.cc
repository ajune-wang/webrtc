/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test.h"
#include "api/optional.h"
#include "rtc_base/criticalsection.h"

#include <stdio.h>
#include <map>
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
  std::ostringstream json_stream;
  if (scalar_.has_value()) {
    json_stream << R"("type":"scalar",)";
    json_stream << R"("value":)" << scalar_.value();
  } else if (mean_and_error_.has_value()) {
    json_stream << R"("type":"list_of_scalars",)";
    json_stream << R"("values":[)" << mean_and_error_.value().first << "],";
    json_stream << R"("std":)" << mean_and_error_.value().second;
  } else {
    json_stream << R"("type":"list_of_scalars",)";
    json_stream << R"("values":[)";
    if (!list_of_scalars_.value().empty()) {
      for (auto it = list_of_scalars_->begin(); it != list_of_scalars_->end();
           ++it) {
        if (it != list_of_scalars_->begin())
          json_stream << ',';
        json_stream << *it;
      }
    }
    json_stream << ']';
  }
  return json_stream.str();
}

class PerfResult {
 public:
  // Constructor for scalar values
  PerfResult(const std::string& trace_name,
             const double value,
             const std::string& units)
      : trace_name_(trace_name), value_(value), units_(units) {}
  // Constructor for mean and error.
  PerfResult(const std::string& trace_name,
             const double mean,
             const double error,
             const std::string& units)
      : trace_name_(trace_name), value_(mean, error), units_(units) {}
  // Constructor for list of scalar values
  PerfResult(const std::string& trace_name,
             const std::vector<double>& list_of_scalars,
             const std::string& units)
      : trace_name_(trace_name), value_(list_of_scalars), units_(units) {}
  std::string ToJSON() const;

 private:
  std::string trace_name_;
  PerfValue value_;
  std::string units_;
};

std::string PerfResult::ToJSON() const {
  std::ostringstream json_stream;
  json_stream << "\"" << trace_name_ << "\":{";
  json_stream << value_.ToJSON() << ',';
  json_stream << R"("units":")" << units_ << "\"";
  json_stream << '}';
  return json_stream.str();
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
    for (auto it = values.begin(); it != values.end(); ++it) {
      if (it != values.begin())
        value_stream << ',';
      value_stream << *it;
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
  std::ostringstream json_stream;
  json_stream << R"({"format_version":"1.0",)";
  json_stream << R"("charts":{)";
  rtc::CritScope lock(&crit_);
  for (auto graphs_it = graphs_.begin(); graphs_it != graphs_.end();
       ++graphs_it) {
    if (graphs_it != graphs_.begin())
      json_stream << ',';
    json_stream << '"' << graphs_it->first << "\":{";
    for (auto traces_it = graphs_it->second.begin();
         traces_it != graphs_it->second.end(); ++traces_it) {
      if (traces_it != graphs_it->second.begin())
        json_stream << ',';
      json_stream << traces_it->ToJSON();
    }
    json_stream << '}';
  }
  json_stream << "}}";
  return json_stream.str();
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
  GetPerfResultsLogger().LogResultMeanAndError(measurement + modifier, trace,
                                               mean, error, units, important);
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
