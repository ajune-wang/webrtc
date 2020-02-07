/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/overuse_frame_detector.h"

#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/field_trial.h"

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <mach/mach.h>
#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

namespace webrtc {

namespace {
const int64_t kCheckForOveruseIntervalMs = 5000;
const int64_t kTimeToFirstCheckForOveruseMs = 100;

// Delay between consecutive rampups. (Used for quick recovery.)
const int kQuickRampUpDelayMs = 10 * 1000;
// Delay between rampup attempts. Initially uses standard, scales up to max.
const int kStandardRampUpDelayMs = 40 * 1000;
const int kMaxRampUpDelayMs = 240 * 1000;
// Expontential back-off factor, to prevent annoying up-down behaviour.
const double kRampUpBackoffFactor = 2.0;

// Max number of overuses detected before always applying the rampup delay.
const int kMaxOverusesBeforeApplyRampupDelay = 4;

const auto kScaleReasonCpu = AdaptationObserverInterface::AdaptReason::kCpu;

// Class for calculating the processing usage on the send-side (roughly the
// average processing time of a frame divided by the average time difference
// between captured frames).

class SendProcessingUsage : public OveruseFrameDetector::ProcessingUsage {
 public:
  explicit SendProcessingUsage(const CpuOveruseOptions& options)
      : options_(options) {
    RTC_CHECK_GT(options.filter_time_ms, 0);
    Reset();
  }
  ~SendProcessingUsage() override = default;

  void Reset() override {
    prev_time_us_ = -1;
    // Start in between the underuse and overuse threshold.
    load_estimate_ = (options_.low_encode_usage_threshold_percent +
                      options_.high_encode_usage_threshold_percent) /
                     200.0;
  }

  absl::optional<int> FrameSent(
      int64_t capture_time_us,
      absl::optional<int> encode_duration_us) override {
    if (encode_duration_us) {
      int duration_per_frame_us =
          DurationPerInputFrame(capture_time_us, *encode_duration_us);
      if (prev_time_us_ != -1) {
        if (capture_time_us < prev_time_us_) {
          // The weighting in AddSample assumes that samples are processed with
          // non-decreasing measurement timestamps. We could implement
          // appropriate weights for samples arriving late, but since it is a
          // rare case, keep things simple, by just pushing those measurements a
          // bit forward in time.
          capture_time_us = prev_time_us_;
        }
        AddSample(1e-6 * duration_per_frame_us,
                  1e-6 * (capture_time_us - prev_time_us_));
      }
    }
    prev_time_us_ = capture_time_us;

    return encode_duration_us;
  }

 private:
  void AddSample(double encode_time, double diff_time) {
    RTC_CHECK_GE(diff_time, 0.0);

    // Use the filter update
    //
    // load <-- x/d (1-exp (-d/T)) + exp (-d/T) load
    //
    // where we must take care for small d, using the proper limit
    // (1 - exp(-d/tau)) / d = 1/tau - d/2tau^2 + O(d^2)
    double tau = (1e-3 * options_.filter_time_ms);
    double e = diff_time / tau;
    double c;
    if (e < 0.0001) {
      c = (1 - e / 2) / tau;
    } else {
      c = -expm1(-e) / diff_time;
    }
    load_estimate_ = c * encode_time + exp(-e) * load_estimate_;
  }

  int64_t DurationPerInputFrame(int64_t capture_time_us,
                                int64_t encode_time_us) {
    // Discard data on old frames; limit 2 seconds.
    static constexpr int64_t kMaxAge = 2 * rtc::kNumMicrosecsPerSec;
    for (auto it = max_encode_time_per_input_frame_.begin();
         it != max_encode_time_per_input_frame_.end() &&
         it->first < capture_time_us - kMaxAge;) {
      it = max_encode_time_per_input_frame_.erase(it);
    }

    std::map<int64_t, int>::iterator it;
    bool inserted;
    std::tie(it, inserted) = max_encode_time_per_input_frame_.emplace(
        capture_time_us, encode_time_us);
    if (inserted) {
      // First encoded frame for this input frame.
      return encode_time_us;
    }
    if (encode_time_us <= it->second) {
      // Shorter encode time than previous frame (unlikely). Count it as being
      // done in parallel.
      return 0;
    }
    // Record new maximum encode time, and return increase from previous max.
    int increase = encode_time_us - it->second;
    it->second = encode_time_us;
    return increase;
  }

  int Value() override {
    return static_cast<int>(100.0 * load_estimate_ + 0.5);
  }

  const CpuOveruseOptions options_;
  // Indexed by the capture timestamp, used as frame id.
  std::map<int64_t, int> max_encode_time_per_input_frame_;

  int64_t prev_time_us_ = -1;
  double load_estimate_;
};

// Class used for manual testing of overuse, enabled via field trial flag.
class OverdoseInjector : public OveruseFrameDetector::ProcessingUsage {
 public:
  OverdoseInjector(std::unique_ptr<OveruseFrameDetector::ProcessingUsage> usage,
                   int64_t normal_period_ms,
                   int64_t overuse_period_ms,
                   int64_t underuse_period_ms)
      : usage_(std::move(usage)),
        normal_period_ms_(normal_period_ms),
        overuse_period_ms_(overuse_period_ms),
        underuse_period_ms_(underuse_period_ms),
        state_(State::kNormal),
        last_toggling_ms_(-1) {
    RTC_DCHECK_GT(overuse_period_ms, 0);
    RTC_DCHECK_GT(normal_period_ms, 0);
    RTC_LOG(LS_INFO) << "Simulating overuse with intervals " << normal_period_ms
                     << "ms normal mode, " << overuse_period_ms
                     << "ms overuse mode.";
  }

  ~OverdoseInjector() override {}

  void Reset() override { usage_->Reset(); }

  absl::optional<int> FrameSent(
      // And these two by the new estimator.
      int64_t capture_time_us,
      absl::optional<int> encode_duration_us) override {
    return usage_->FrameSent(capture_time_us, encode_duration_us);
  }

  int Value() override {
    int64_t now_ms = rtc::TimeMillis();
    if (last_toggling_ms_ == -1) {
      last_toggling_ms_ = now_ms;
    } else {
      switch (state_) {
        case State::kNormal:
          if (now_ms > last_toggling_ms_ + normal_period_ms_) {
            state_ = State::kOveruse;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Simulating CPU overuse.";
          }
          break;
        case State::kOveruse:
          if (now_ms > last_toggling_ms_ + overuse_period_ms_) {
            state_ = State::kUnderuse;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Simulating CPU underuse.";
          }
          break;
        case State::kUnderuse:
          if (now_ms > last_toggling_ms_ + underuse_period_ms_) {
            state_ = State::kNormal;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Actual CPU overuse measurements in effect.";
          }
          break;
      }
    }

    absl::optional<int> overried_usage_value;
    switch (state_) {
      case State::kNormal:
        break;
      case State::kOveruse:
        overried_usage_value.emplace(250);
        break;
      case State::kUnderuse:
        overried_usage_value.emplace(5);
        break;
    }

    return overried_usage_value.value_or(usage_->Value());
  }

 private:
  const std::unique_ptr<OveruseFrameDetector::ProcessingUsage> usage_;
  const int64_t normal_period_ms_;
  const int64_t overuse_period_ms_;
  const int64_t underuse_period_ms_;
  enum class State { kNormal, kOveruse, kUnderuse } state_;
  int64_t last_toggling_ms_;
};

}  // namespace

CpuOveruseOptions::CpuOveruseOptions()
    : high_encode_usage_threshold_percent(85),
      frame_timeout_interval_ms(1500),
      min_process_count(3),
      high_threshold_consecutive_count(2),
      filter_time_ms(5 * rtc::kNumMillisecsPerSec) {
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  // This is proof-of-concept code for letting the physical core count affect
  // the interval into which we attempt to scale. For now, the code is Mac OS
  // specific, since that's the platform were we saw most problems.
  // TODO(torbjorng): Enhance SystemInfo to return this metric.

  mach_port_t mach_host = mach_host_self();
  host_basic_info hbi = {};
  mach_msg_type_number_t info_count = HOST_BASIC_INFO_COUNT;
  kern_return_t kr =
      host_info(mach_host, HOST_BASIC_INFO, reinterpret_cast<host_info_t>(&hbi),
                &info_count);
  mach_port_deallocate(mach_task_self(), mach_host);

  int n_physical_cores;
  if (kr != KERN_SUCCESS) {
    // If we couldn't get # of physical CPUs, don't panic. Assume we have 1.
    n_physical_cores = 1;
    RTC_LOG(LS_ERROR)
        << "Failed to determine number of physical cores, assuming 1";
  } else {
    n_physical_cores = hbi.physical_cpu;
    RTC_LOG(LS_INFO) << "Number of physical cores:" << n_physical_cores;
  }

  // Change init list default for few core systems. The assumption here is that
  // encoding, which we measure here, takes about 1/4 of the processing of a
  // two-way call. This is roughly true for x86 using both vp8 and vp9 without
  // hardware encoding. Since we don't affect the incoming stream here, we only
  // control about 1/2 of the total processing needs, but this is not taken into
  // account.
  if (n_physical_cores == 1)
    high_encode_usage_threshold_percent = 20;  // Roughly 1/4 of 100%.
  else if (n_physical_cores == 2)
    high_encode_usage_threshold_percent = 40;  // Roughly 1/4 of 200%.
#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

  // Note that we make the interval 2x+epsilon wide, since libyuv scaling steps
  // are close to that (when squared). This wide interval makes sure that
  // scaling up or down does not jump all the way across the interval.
  low_encode_usage_threshold_percent =
      (high_encode_usage_threshold_percent - 1) / 2;
}

std::unique_ptr<OveruseFrameDetector::ProcessingUsage>
OveruseFrameDetector::CreateProcessingUsage(const CpuOveruseOptions& options) {
  std::unique_ptr<ProcessingUsage> instance;
  instance = std::make_unique<SendProcessingUsage>(options);

  std::string toggling_interval =
      field_trial::FindFullName("WebRTC-ForceSimulatedOveruseIntervalMs");
  if (!toggling_interval.empty()) {
    int normal_period_ms = 0;
    int overuse_period_ms = 0;
    int underuse_period_ms = 0;
    if (sscanf(toggling_interval.c_str(), "%d-%d-%d", &normal_period_ms,
               &overuse_period_ms, &underuse_period_ms) == 3) {
      if (normal_period_ms > 0 && overuse_period_ms > 0 &&
          underuse_period_ms > 0) {
        instance = std::make_unique<OverdoseInjector>(
            std::move(instance), normal_period_ms, overuse_period_ms,
            underuse_period_ms);
      } else {
        RTC_LOG(LS_WARNING)
            << "Invalid (non-positive) normal/overuse/underuse periods: "
            << normal_period_ms << " / " << overuse_period_ms << " / "
            << underuse_period_ms;
      }
    } else {
      RTC_LOG(LS_WARNING) << "Malformed toggling interval: "
                          << toggling_interval;
    }
  }
  return instance;
}

OveruseFrameDetector::OveruseFrameDetector(
    CpuOveruseMetricsObserver* metrics_observer)
    : metrics_observer_(metrics_observer),
      num_process_times_(0),
      // TODO(nisse): Use absl::optional
      last_capture_time_us_(-1),
      num_pixels_(0),
      last_overuse_time_ms_(-1),
      checks_above_threshold_(0),
      num_overuse_detections_(0),
      last_rampup_time_ms_(-1),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs) {
  task_checker_.Detach();
  ParseFieldTrial({&filter_time_constant_},
                  field_trial::FindFullName("WebRTC-CpuLoadEstimator"));
}

OveruseFrameDetector::~OveruseFrameDetector() {}

void OveruseFrameDetector::StartCheckForOveruse(
    TaskQueueBase* task_queue_base,
    const CpuOveruseOptions& options,
    AdaptationObserverInterface* overuse_observer) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  RTC_DCHECK(!check_overuse_task_.Running());
  RTC_DCHECK(overuse_observer != nullptr);

  SetOptions(options);
  check_overuse_task_ = RepeatingTaskHandle::DelayedStart(
      task_queue_base, TimeDelta::ms(kTimeToFirstCheckForOveruseMs),
      [this, overuse_observer] {
        CheckForOveruse(overuse_observer);
        return TimeDelta::ms(kCheckForOveruseIntervalMs);
      });
}
void OveruseFrameDetector::StopCheckForOveruse() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  check_overuse_task_.Stop();
}

void OveruseFrameDetector::EncodedFrameTimeMeasured(int encode_duration_ms) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  encode_usage_percent_ = usage_->Value();

  metrics_observer_->OnEncodedFrameTimeMeasured(encode_duration_ms,
                                                *encode_usage_percent_);
}

bool OveruseFrameDetector::FrameSizeChanged(int num_pixels) const {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (num_pixels != num_pixels_) {
    return true;
  }
  return false;
}

bool OveruseFrameDetector::FrameTimeoutDetected(int64_t now_us) const {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (last_capture_time_us_ == -1)
    return false;
  return (now_us - last_capture_time_us_) >
         options_.frame_timeout_interval_ms * rtc::kNumMicrosecsPerMillisec;
}

void OveruseFrameDetector::ResetAll(int num_pixels) {
  // Reset state, as a result resolution being changed. Do not however change
  // the current frame rate back to the default.
  RTC_DCHECK_RUN_ON(&task_checker_);
  num_pixels_ = num_pixels;
  usage_->Reset();
  last_capture_time_us_ = -1;
  num_process_times_ = 0;
  encode_usage_percent_ = absl::nullopt;
}

void OveruseFrameDetector::FrameSent(int64_t capture_time_us,
                                     absl::optional<int> encode_duration_us) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  encode_duration_us = usage_->FrameSent(capture_time_us, encode_duration_us);

  if (encode_duration_us) {
    EncodedFrameTimeMeasured(*encode_duration_us /
                             rtc::kNumMicrosecsPerMillisec);
  }
}

void OveruseFrameDetector::CheckForOveruse(
    AdaptationObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  RTC_DCHECK(observer);
  ++num_process_times_;
  if (num_process_times_ <= options_.min_process_count ||
      !encode_usage_percent_)
    return;

  int64_t now_ms = rtc::TimeMillis();

  if (IsOverusing(*encode_usage_percent_)) {
    // If the last thing we did was going up, and now have to back down, we need
    // to check if this peak was short. If so we should back off to avoid going
    // back and forth between this load, the system doesn't seem to handle it.
    bool check_for_backoff = last_rampup_time_ms_ > last_overuse_time_ms_;
    if (check_for_backoff) {
      if (now_ms - last_rampup_time_ms_ < kStandardRampUpDelayMs ||
          num_overuse_detections_ > kMaxOverusesBeforeApplyRampupDelay) {
        // Going up was not ok for very long, back off.
        current_rampup_delay_ms_ *= kRampUpBackoffFactor;
        if (current_rampup_delay_ms_ > kMaxRampUpDelayMs)
          current_rampup_delay_ms_ = kMaxRampUpDelayMs;
      } else {
        // Not currently backing off, reset rampup delay.
        current_rampup_delay_ms_ = kStandardRampUpDelayMs;
      }
    }

    last_overuse_time_ms_ = now_ms;
    in_quick_rampup_ = false;
    checks_above_threshold_ = 0;
    ++num_overuse_detections_;

    observer->AdaptDown(kScaleReasonCpu);
  } else if (IsUnderusing(*encode_usage_percent_, now_ms)) {
    last_rampup_time_ms_ = now_ms;
    in_quick_rampup_ = true;

    observer->AdaptUp(kScaleReasonCpu);
  }

  int rampup_delay =
      in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;

  RTC_LOG(LS_VERBOSE) << " Frame stats: "
                         " encode usage "
                      << *encode_usage_percent_ << " overuse detections "
                      << num_overuse_detections_ << " rampup delay "
                      << rampup_delay;
}

void OveruseFrameDetector::SetOptions(const CpuOveruseOptions& options) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  options_ = options;

  // Time constant config overridable by field trial.
  if (filter_time_constant_) {
    options_.filter_time_ms = filter_time_constant_->ms();
  }
  // Force reset with next frame.
  num_pixels_ = 0;
  usage_ = CreateProcessingUsage(options);
}

bool OveruseFrameDetector::IsOverusing(int usage_percent) {
  RTC_DCHECK_RUN_ON(&task_checker_);

  if (usage_percent >= options_.high_encode_usage_threshold_percent) {
    ++checks_above_threshold_;
  } else {
    checks_above_threshold_ = 0;
  }
  return checks_above_threshold_ >= options_.high_threshold_consecutive_count;
}

bool OveruseFrameDetector::IsUnderusing(int usage_percent, int64_t time_now) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  int delay = in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  if (time_now < last_rampup_time_ms_ + delay)
    return false;

  return usage_percent < options_.low_encode_usage_threshold_percent;
}
}  // namespace webrtc
