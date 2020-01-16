/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test_histogram_writer.h"

#include <stdlib.h>

#include <map>
#include <memory>

#include "rtc_base/critical_section.h"
#include "rtc_base/logging.h"
#include "third_party/catapult/tracing/tracing/value/histogram.h"
#include "third_party/catapult/tracing/tracing/value/histogram_json_converter.h"

namespace webrtc {
namespace test {

namespace {

namespace proto = catapult::tracing::tracing::proto;

std::string AsJsonString(const std::string string) {
  return "\"" + string + "\"";
}

class PerfTestHistogramWriter : public PerfTestResultWriter {
 public:
  PerfTestHistogramWriter() : crit_() {}
  void ClearResults() override {
    rtc::CritScope lock(&crit_);
    histograms_.clear();
  }

  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important,
                 ImproveDirection improve_direction) override {
    AddSample(graph_name, trace_name, value, units, important,
              improve_direction);
  }
  void LogResultMeanAndError(const std::string& graph_name,
                             const std::string& trace_name,
                             const double mean,
                             const double error,
                             const std::string& units,
                             const bool important,
                             ImproveDirection improve_direction) override {
    RTC_LOG(LS_WARNING) << "Discarding stddev, not supported by histograms";
    (void)error;

    AddSample(graph_name, trace_name, mean, units, important,
              improve_direction);
  }
  void LogResultList(const std::string& graph_name,
                     const std::string& trace_name,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     const bool important,
                     ImproveDirection improve_direction) override {
    for (double value : values) {
      AddSample(graph_name, trace_name, value, units, important,
                improve_direction);
    }
  }
  std::string ToJSON() const override {
    proto::HistogramSet histogram_set;

    rtc::CritScope lock(&crit_);
    for (const auto& histogram : histograms_) {
      std::unique_ptr<proto::Histogram> proto = histogram.second->toProto();
      histogram_set.mutable_histograms()->AddAllocated(proto.release());
    }

    std::string output;
    bool ok = catapult::ToJson(histogram_set, &output);
    RTC_DCHECK(ok) << "Failed to convert histogram set to JSON";
    return output;
  }

 private:
  void AddSample(const std::string& original_graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important,
                 ImproveDirection improve_direction) {
    (void)important;  // Ignored on purpose.

    // WebRTC annotates the units into the metric name when they are not
    // supported by the Histogram API.
    std::string graph_name = original_graph_name;
    if (units == "dB") {
      graph_name += "_dB";
    } else if (units == "fps") {
      graph_name += "_fps";
    } else if (units == "%") {
      graph_name += "_%";
    }

    rtc::CritScope lock(&crit_);
    if (histograms_.count(graph_name) == 0) {
      proto::UnitAndDirection unit = ParseUnit(units, improve_direction);
      std::unique_ptr<catapult::HistogramBuilder> builder =
          std::make_unique<catapult::HistogramBuilder>(graph_name, unit);
      histograms_[graph_name] = std::move(builder);

      proto::Diagnostic stories;
      proto::GenericSet* generic_set = stories.mutable_generic_set();
      generic_set->add_values(AsJsonString(trace_name));
      histograms_[graph_name]->AddDiagnostic(catapult::kStoriesDiagnostic,
                                             stories);
    }

    if (units == "bps") {
      // Bps has been interpreted as bits per second in WebRTC tests.
      histograms_[graph_name]->AddSample(value / 8);
    } else {
      histograms_[graph_name]->AddSample(value);
    }
  }

  proto::UnitAndDirection ParseUnit(const std::string& units,
                                    ImproveDirection improve_direction) {
    RTC_DCHECK(units.find('_') == std::string::npos)
        << "The unit_bigger|smallerIsBetter syntax isn't supported in WebRTC, "
           "use the enum instead.";

    proto::UnitAndDirection unit;
    unit.set_improvement_direction(ParseDirection(improve_direction));
    if (units == "bps") {
      unit.set_unit(proto::BYTES_PER_SECOND);
    } else if (units == "dB") {
      unit.set_unit(proto::UNITLESS);
    } else if (units == "fps") {
      unit.set_unit(proto::HERTZ);
    } else if (units == "frames") {
      unit.set_unit(proto::COUNT);
    } else if (units == "ms") {
      unit.set_unit(proto::MS_BEST_FIT_FORMAT);
    } else if (units == "%") {
      unit.set_unit(proto::UNITLESS);
    } else {
      unit.set_unit(catapult::UnitFromJsonUnit(units));
    }
    return unit;
  }

  proto::ImprovementDirection ParseDirection(
      ImproveDirection improve_direction) {
    switch (improve_direction) {
      case ImproveDirection::kNone:
        return proto::NOT_SPECIFIED;
      case ImproveDirection::kSmallerIsBetter:
        return proto::SMALLER_IS_BETTER;
      case ImproveDirection::kBiggerIsBetter:
        return proto::BIGGER_IS_BETTER;
      default:
        RTC_NOTREACHED() << "Invalid enum value " << improve_direction;
    }
  }

 private:
  rtc::CriticalSection crit_;
  std::map<std::string, std::unique_ptr<catapult::HistogramBuilder>> histograms_
      RTC_GUARDED_BY(&crit_);
};

}  // namespace

PerfTestResultWriter* CreateHistogramWriter() {
  return new PerfTestHistogramWriter();
}

}  // namespace test
}  // namespace webrtc
