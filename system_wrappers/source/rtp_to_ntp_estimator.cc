/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/rtp_to_ntp_estimator.h"

#include <stddef.h>

#include <cmath>
#include <vector>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// Maximum number of RTCP SR reports to use to map between RTP and NTP.
constexpr size_t kNumRtcpReportsToUse = 20;
// Don't allow NTP timestamps to jump more than 1 hour. Chosen arbitrary as big
// enough to not affect normal use-cases. Yet it is smaller than RTP wrap-around
// half-period (90khz RTP clock wrap-arounds every 13.25 hours). After half of
// wrap-around period it is impossible to unwrap RTP timestamps correctly.
constexpr uint64_t kMaxAllowedRtcpNtpInterval = uint64_t{60 * 60} << 32;

// RtcpMeasurement is a private type in the RtpToNtpEstimator, thus use
// template hack to access it using this helper function.
template <typename RtcpMeasurement>
bool Contains(const std::list<RtcpMeasurement>& measurements,
              const RtcpMeasurement& other) {
  for (const RtcpMeasurement& measurement : measurements) {
    // Use || since two equal timestamps will result in zero frequency and in
    // RtpToNtpMs, `rtp_timestamp_ms` is estimated by dividing by the frequency.
    if (measurement.ntp_time == other.ntp_time ||
        measurement.unwrapped_rtp_timestamp == other.unwrapped_rtp_timestamp) {
      return true;
    }
  }
  return false;
}

// Given x[] and y[] writes out such k and b that line y=k*x+b approximates
// given points in the best way (Least Squares Method).
bool LinearRegression(rtc::ArrayView<const double> x,
                      rtc::ArrayView<const double> y,
                      double* k,
                      double* b) {
  size_t n = x.size();
  if (n < 2)
    return false;

  if (y.size() != n)
    return false;

  double avg_x = 0;
  double avg_y = 0;
  for (size_t i = 0; i < n; ++i) {
    avg_x += x[i];
    avg_y += y[i];
  }
  avg_x /= n;
  avg_y /= n;

  double variance_x = 0;
  double covariance_xy = 0;
  for (size_t i = 0; i < n; ++i) {
    double normalized_x = x[i] - avg_x;
    double normalized_y = y[i] - avg_y;
    variance_x += normalized_x * normalized_x;
    covariance_xy += normalized_x * normalized_y;
  }

  if (std::fabs(variance_x) < 1e-8)
    return false;

  *k = static_cast<double>(covariance_xy / variance_x);
  *b = static_cast<double>(avg_y - (*k) * avg_x);
  return true;
}

}  // namespace

void RtpToNtpEstimator::UpdateParameters() {
  if (measurements_.size() < 2)
    return;

  std::vector<double> x;
  std::vector<double> y;
  x.reserve(measurements_.size());
  y.reserve(measurements_.size());
  for (auto it = measurements_.begin(); it != measurements_.end(); ++it) {
    x.push_back(it->unwrapped_rtp_timestamp);
    y.push_back(static_cast<uint64_t>(it->ntp_time));
  }
  Parameters p;

  if (!LinearRegression(x, y, &p.slope, &p.offset)) {
    return;
  }

  params_ = p;
}

RtpToNtpEstimator::UpdateResult RtpToNtpEstimator::UpdateMeasurements(
    NtpTime ntp,
    uint32_t rtp_timestamp) {
  int64_t unwrapped_rtp_timestamp = unwrapper_.Unwrap(rtp_timestamp);

  RtcpMeasurement new_measurement = {
      .ntp_time = ntp, .unwrapped_rtp_timestamp = unwrapped_rtp_timestamp};

  if (Contains(measurements_, new_measurement)) {
    // RTCP SR report already added.
    return kSameMeasurement;
  }

  if (!new_measurement.ntp_time.Valid())
    return kInvalidMeasurement;

  uint64_t ntp_new = static_cast<uint64_t>(new_measurement.ntp_time);
  bool invalid_sample = false;
  if (!measurements_.empty()) {
    int64_t old_rtp_timestamp = measurements_.front().unwrapped_rtp_timestamp;
    uint64_t old_ntp = static_cast<uint64_t>(measurements_.front().ntp_time);
    if (ntp_new <= old_ntp || ntp_new > old_ntp + kMaxAllowedRtcpNtpInterval) {
      invalid_sample = true;
    } else if (unwrapped_rtp_timestamp <= old_rtp_timestamp) {
      RTC_LOG(LS_WARNING)
          << "Newer RTCP SR report with older RTP timestamp, dropping";
      invalid_sample = true;
    } else if (unwrapped_rtp_timestamp - old_rtp_timestamp > (1 << 25)) {
      // Sanity check. No jumps too far into the future in rtp.
      invalid_sample = true;
    }
  }

  if (invalid_sample) {
    ++consecutive_invalid_samples_;
    if (consecutive_invalid_samples_ < kMaxInvalidSamples) {
      return kInvalidMeasurement;
    }
    RTC_LOG(LS_WARNING) << "Multiple consecutively invalid RTCP SR reports, "
                           "clearing measurements.";
    measurements_.clear();
    params_ = absl::nullopt;
  }
  consecutive_invalid_samples_ = 0;

  // Insert new RTCP SR report.
  if (measurements_.size() == kNumRtcpReportsToUse)
    measurements_.pop_back();

  measurements_.push_front(new_measurement);

  // List updated, calculate new parameters.
  UpdateParameters();
  return kNewMeasurement;
}

NtpTime RtpToNtpEstimator::Estimate(uint32_t rtp_timestamp) const {
  if (!params_)
    return NtpTime();

  double estimated =
      static_cast<double>(unwrapper_.Unwrap(rtp_timestamp)) * params_->slope +
      params_->offset + 0.5f;

  if (estimated < 0)
    return NtpTime();

  return NtpTime(static_cast<uint64_t>(estimated));
}

double RtpToNtpEstimator::EstimatedFrequencyKhz() const {
  if (!params_.has_value()) {
    return 0.0;
  }
  static constexpr double kNtpUnitPerMs = 4.294967296E6;  // 2^32 / 1000.
  return kNtpUnitPerMs / params_->slope;
}

}  // namespace webrtc
