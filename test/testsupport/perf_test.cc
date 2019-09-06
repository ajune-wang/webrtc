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

#include <stdio.h>

#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/samples_stats_counter.h"

namespace webrtc {
namespace test {
namespace {

template <typename Container>
void OutputListToStream(std::ostream* ostream, const Container& values) {
  const char* sep = "";
  for (const auto& v : values) {
    (*ostream) << sep << v;
    sep = ",";
  }
}

class PerfResultsLoggerInterface {
 public:
  virtual ~PerfResultsLoggerInterface() = default;

  virtual void ClearResults() = 0;
  virtual void LogResult(const std::string& graph_name,
                         const std::string& trace_name,
                         const SamplesStatsCounter& counter,
                         const std::string& units,
                         const bool important) = 0;
  virtual void LogResult(const std::string& graph_name,
                         const std::string& trace_name,
                         const double value,
                         const std::string& units,
                         const bool important) = 0;
  virtual void LogResultMeanAndError(const std::string& graph_name,
                                     const std::string& trace_name,
                                     const double mean,
                                     const double error,
                                     const std::string& units,
                                     const bool important) = 0;
  virtual void LogResultList(const std::string& graph_name,
                             const std::string& trace_name,
                             const rtc::ArrayView<const double> values,
                             const std::string& units,
                             const bool important) = 0;
};

class StdOutPerfResultsLogger : public PerfResultsLoggerInterface {
 public:
  StdOutPerfResultsLogger() : output_(stdout) {}
  ~StdOutPerfResultsLogger() override = default;

  void SetOutput(FILE* output) {
    rtc::CritScope lock(&crit_);
    output_ = output;
  }

  void ClearResults() override {}
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const SamplesStatsCounter& counter,
                 const std::string& units,
                 const bool important) override {
    LogResultMeanAndError(
        graph_name, trace_name, counter.IsEmpty() ? 0 : counter.GetAverage(),
        counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), units,
        important);
  }
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important) override {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << value;
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units,
                   important);
  }
  void LogResultMeanAndError(const std::string& graph_name,
                             const std::string& trace_name,
                             const double mean,
                             const double error,
                             const std::string& units,
                             const bool important) override {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << '{' << mean << ',' << error << '}';
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units,
                   important);
  }
  void LogResultList(const std::string& graph_name,
                     const std::string& trace_name,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     const bool important) override {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << '[';
    OutputListToStream(&value_stream, values);
    value_stream << ']';
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units,
                   important);
  }

 private:
  void LogResultsImpl(const std::string& graph_name,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& units,
                      bool important) {
    // <*>RESULT <graph_name>: <trace_name>= <value> <units>
    // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
    // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
    rtc::CritScope lock(&crit_);

    if (important) {
      fprintf(output_, "*");
    }
    fprintf(output_, "RESULT %s: %s= %s %s\n", graph_name.c_str(),
            trace.c_str(), values.c_str(), units.c_str());
  }

  rtc::CriticalSection crit_;
  FILE* output_ RTC_GUARDED_BY(&crit_);
};

class JsonPerfResultsLogger : public PerfResultsLoggerInterface {
 public:
  JsonPerfResultsLogger() = default;
  ~JsonPerfResultsLogger() override = default;

  void ClearResults() override {
    rtc::CritScope lock(&crit_);
    graphs_.clear();
  }

  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const SamplesStatsCounter& counter,
                 const std::string& units,
                 const bool important) override {
    LogResultMeanAndError(
        graph_name, trace_name, counter.IsEmpty() ? 0 : counter.GetAverage(),
        counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), units,
        important);
  }
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important) override {
    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"scalar",)";
    json_stream << R"("value":)" << value << ',';
    json_stream << R"("units":")" << units << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  void LogResultMeanAndError(const std::string& graph_name,
                             const std::string& trace_name,
                             const double mean,
                             const double error,
                             const std::string& units,
                             const bool important) override {
    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"list_of_scalar_values",)";
    json_stream << R"("values":[)" << mean << "],";
    json_stream << R"("std":)" << error << ',';
    json_stream << R"("units":")" << units << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  void LogResultList(const std::string& graph_name,
                     const std::string& trace_name,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     const bool important) override {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << '[';
    OutputListToStream(&value_stream, values);
    value_stream << ']';

    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"list_of_scalar_values",)";
    json_stream << R"("values":)" << value_stream.str() << ',';
    json_stream << R"("units":")" << units << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  std::string ToJSON() const {
    rtc::CritScope lock(&crit_);
    std::ostringstream json_stream;
    json_stream << R"({"format_version":"1.0",)";
    json_stream << R"("charts":{)";
    for (auto graphs_it = graphs_.begin(); graphs_it != graphs_.end();
         ++graphs_it) {
      if (graphs_it != graphs_.begin())
        json_stream << ',';
      json_stream << '"' << graphs_it->first << "\":";
      json_stream << '{';
      OutputListToStream(&json_stream, graphs_it->second);
      json_stream << '}';
    }
    json_stream << "}}";
    return json_stream.str();
  }

 private:
  rtc::CriticalSection crit_;
  std::map<std::string, std::vector<std::string>> graphs_
      RTC_GUARDED_BY(&crit_);
};

std::string ToString(PerfLoggingFeature feature) {
  switch (feature) {
    case PerfLoggingFeature::kStdOut:
      return "StdOut";
    case PerfLoggingFeature::kJson:
      return "Json";
  }
}

class FeaturedPerfResultsLogger : public PerfResultsLoggerInterface {
 public:
  FeaturedPerfResultsLogger() { EnableFeature(PerfLoggingFeature::kStdOut); }
  ~FeaturedPerfResultsLogger() override = default;

  void EnableFeature(PerfLoggingFeature feature) {
    rtc::CritScope crit(&lock_);
    if (IsFeatureEnabled(feature)) {
      RTC_LOG(WARNING) << "Perf logging feature [" << ToString(feature)
                       << "] already enabled";
      return;
    }
    switch (feature) {
      case PerfLoggingFeature::kStdOut:
        loggers_.insert(
            {feature, absl::make_unique<StdOutPerfResultsLogger>()});
        break;
      case PerfLoggingFeature::kJson:
        loggers_.insert({feature, absl::make_unique<JsonPerfResultsLogger>()});
        break;
    }
  }
  void DisableFeature(PerfLoggingFeature feature) {
    rtc::CritScope crit(&lock_);
    if (!IsFeatureEnabled(feature)) {
      RTC_LOG(WARNING) << "Perf logging feature [" << ToString(feature)
                       << "] already disabled";
      return;
    }
    loggers_.erase(feature);
  }
  void SetOutput(FILE* output) {
    rtc::CritScope crit(&lock_);
    RTC_CHECK(IsFeatureEnabled(PerfLoggingFeature::kStdOut))
        << "Perf logging feature [" << ToString(PerfLoggingFeature::kStdOut)
        << "] have to be enabled";
    static_cast<StdOutPerfResultsLogger*>(
        loggers_.at(PerfLoggingFeature::kStdOut).get())
        ->SetOutput(output);
  }
  std::string ToJSON() const {
    rtc::CritScope crit(&lock_);
    RTC_CHECK(IsFeatureEnabled(PerfLoggingFeature::kJson))
        << "Perf logging feature [" << ToString(PerfLoggingFeature::kJson)
        << "] have to be enabled";
    return static_cast<JsonPerfResultsLogger*>(
               loggers_.at(PerfLoggingFeature::kJson).get())
        ->ToJSON();
  }

  void ClearResults() override {
    rtc::CritScope crit(&lock_);
    for (auto& logger_entry : loggers_) {
      logger_entry.second->ClearResults();
    }
  }
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const SamplesStatsCounter& counter,
                 const std::string& units,
                 const bool important) override {
    RTC_CHECK(std::isfinite(counter.IsEmpty() ? 0 : counter.GetAverage()));
    RTC_CHECK(
        std::isfinite(counter.IsEmpty() ? 0 : counter.GetStandardDeviation()));

    rtc::CritScope crit(&lock_);
    for (auto& logger_entry : loggers_) {
      logger_entry.second->LogResult(graph_name, trace_name, counter, units,
                                     important);
    }
  }
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important) override {
    RTC_CHECK(std::isfinite(value))
        << "Expected finite value for graph " << graph_name << ", trace name "
        << trace_name << ", units " << units << ", got " << value;

    rtc::CritScope crit(&lock_);
    for (auto& logger_entry : loggers_) {
      logger_entry.second->LogResult(graph_name, trace_name, value, units,
                                     important);
    }
  }
  void LogResultMeanAndError(const std::string& graph_name,
                             const std::string& trace_name,
                             const double mean,
                             const double error,
                             const std::string& units,
                             const bool important) override {
    RTC_CHECK(std::isfinite(mean));
    RTC_CHECK(std::isfinite(error));

    rtc::CritScope crit(&lock_);
    for (auto& logger_entry : loggers_) {
      logger_entry.second->LogResultMeanAndError(graph_name, trace_name, mean,
                                                 error, units, important);
    }
  }
  void LogResultList(const std::string& graph_name,
                     const std::string& trace_name,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     const bool important) override {
    for (double v : values) {
      RTC_CHECK(std::isfinite(v));
    }

    rtc::CritScope crit(&lock_);
    for (auto& logger_entry : loggers_) {
      logger_entry.second->LogResultList(graph_name, trace_name, values, units,
                                         important);
    }
  }

 private:
  bool IsFeatureEnabled(PerfLoggingFeature feature) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return loggers_.find(feature) != loggers_.end();
  }

  rtc::CriticalSection lock_;
  std::map<PerfLoggingFeature, std::unique_ptr<PerfResultsLoggerInterface>>
      loggers_;
};

FeaturedPerfResultsLogger& GetPerfResultsLogger() {
  static FeaturedPerfResultsLogger* const logger_ =
      new FeaturedPerfResultsLogger();
  return *logger_;
}

}  // namespace

void ClearPerfResults() {
  GetPerfResultsLogger().ClearResults();
}

void EnablePerfLoggingFeature(PerfLoggingFeature feature) {
  GetPerfResultsLogger().EnableFeature(feature);
}

void DisablePerfLoggingFeature(PerfLoggingFeature feature) {
  GetPerfResultsLogger().DisableFeature(feature);
}

void SetPerfResultsOutput(FILE* output) {
  GetPerfResultsLogger().SetOutput(output);
}

std::string GetPerfResultsJSON() {
  return GetPerfResultsLogger().ToJSON();
}

void WritePerfResults(const std::string& output_path) {
  std::string json_results = GetPerfResultsJSON();
  std::fstream json_file(output_path, std::fstream::out);
  json_file << json_results;
  json_file.close();
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
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     bool important) {
  GetPerfResultsLogger().LogResultList(measurement + modifier, trace, values,
                                       units, important);
}

}  // namespace test
}  // namespace webrtc
