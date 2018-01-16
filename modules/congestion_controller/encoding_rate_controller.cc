/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/encoding_rate_controller.h"

#include <algorithm>

#include "network_control/include/network_units.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

using webrtc::network::units::Timestamp;
using webrtc::network::units::TimeDelta;
using webrtc::network::units::DataSize;
using webrtc::network::units::DataRate;

namespace webrtc {
namespace network {
namespace {
const char kPacerPushbackExperiment[] = "WebRTC-PacerPushbackExperiment";
static const int64_t kRetransmitWindowSizeMs = 500;
}  // namespace

class EncodingRateController::Impl {
 public:
  explicit Impl(const Clock* clock);
  ~Impl() = default;

 public:
  void OnNetworkAvailability(NetworkAvailability);
  void OnTargetTransferRate(TargetTransferRate);
  void OnPacerQueueUpdate(PacerQueueUpdate);

  void OnNetworkInvalidation();

  bool GetNetworkParameters(int32_t* estimated_bitrate_bps,
                            uint8_t* fraction_loss,
                            int64_t* rtt_ms);
  bool IsSendQueueFull() const;
  bool HasNetworkParametersToReportChanged(int64_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  rtc::CriticalSection observer_lock_;
  SendSideCongestionController::Observer* observer_
      RTC_GUARDED_BY(observer_lock_) = nullptr;
  const std::unique_ptr<RateLimiter> retransmission_rate_limiter_;

  rtc::Optional<TargetTransferRate> current_target_rate_;

  bool network_available_ = true;
  int64_t last_reported_target_bitrate_bps_;
  uint8_t last_reported_fraction_loss_;
  int64_t last_reported_rtt_ms_;
  bool pacer_pushback_experiment_ = false;
  int64_t pacer_expected_queue_ms_ = 0;
  float encoding_rate_ = 1.0;
};

EncodingRateController::EncodingRateController(const Clock* clock)
    : impl_(rtc::MakeUnique<Impl>(clock)) {
  NetworkAvailabilityReceiver.Bind(impl_.get(), &Impl::OnNetworkAvailability);
  PacerQueueUpdateReceiver.Bind(impl_.get(), &Impl::OnPacerQueueUpdate);
  TargetTransferRateReceiver.Bind(impl_.get(), &Impl::OnTargetTransferRate);
}

EncodingRateController::~EncodingRateController() {}

void EncodingRateController::RegisterNetworkObserver(
    SendSideCongestionController::Observer* observer) {
  rtc::CritScope cs(&impl_->observer_lock_);
  RTC_DCHECK(impl_->observer_ == nullptr);
  impl_->observer_ = observer;
}

void EncodingRateController::DeRegisterNetworkObserver(
    SendSideCongestionController::Observer* observer) {
  rtc::CritScope cs(&impl_->observer_lock_);
  RTC_DCHECK_EQ(impl_->observer_, observer);
  impl_->observer_ = nullptr;
}

RateLimiter* EncodingRateController::GetRetransmissionRateLimiter() {
  return impl_->retransmission_rate_limiter_.get();
}

EncodingRateController::Impl::Impl(const Clock* clock)
    : retransmission_rate_limiter_(
          new RateLimiter(clock, kRetransmitWindowSizeMs)),
      last_reported_target_bitrate_bps_(0),
      last_reported_fraction_loss_(0),
      last_reported_rtt_ms_(0),
      pacer_pushback_experiment_(
          webrtc::field_trial::IsEnabled(kPacerPushbackExperiment)) {}

void EncodingRateController::Impl::OnNetworkAvailability(
    NetworkAvailability msg) {
  network_available_ = msg.network_available;
  OnNetworkInvalidation();
}

void EncodingRateController::Impl::OnTargetTransferRate(
    TargetTransferRate target_rate) {
  retransmission_rate_limiter_->SetMaxRate(
      target_rate.basis_estimate.bandwidth.bps());
  current_target_rate_ = target_rate;
  OnNetworkInvalidation();
}

void EncodingRateController::Impl::OnPacerQueueUpdate(PacerQueueUpdate msg) {
  pacer_expected_queue_ms_ = msg.expected_queue_time.ms();
  OnNetworkInvalidation();
}

void EncodingRateController::Impl::OnNetworkInvalidation() {
  uint32_t target_bitrate_bps;
  uint8_t fraction_loss;
  int64_t rtt_ms;

  if (!current_target_rate_.has_value())
    return;
  target_bitrate_bps = current_target_rate_->target_rate.bps();
  fraction_loss = current_target_rate_->basis_estimate.GetLossRatioUint8();
  rtt_ms = current_target_rate_->basis_estimate.round_trip_time.ms();
  int64_t probing_interval_ms =
      current_target_rate_->basis_estimate.bwe_period.ms();

  if (!network_available_) {
    target_bitrate_bps = 0;
  } else if (!pacer_pushback_experiment_) {
    target_bitrate_bps = IsSendQueueFull() ? 0 : target_bitrate_bps;
  } else {
    int64_t queue_length_ms = pacer_expected_queue_ms_;

    if (queue_length_ms == 0) {
      encoding_rate_ = 1.0;
    } else if (queue_length_ms > 50) {
      float encoding_rate = 1.0 - queue_length_ms / 1000.0;
      encoding_rate_ = std::min(encoding_rate_, encoding_rate);
      encoding_rate_ = std::max(encoding_rate_, 0.0f);
    }

    target_bitrate_bps *= encoding_rate_;
    target_bitrate_bps = target_bitrate_bps < 50000 ? 0 : target_bitrate_bps;
  }
  if (HasNetworkParametersToReportChanged(target_bitrate_bps, fraction_loss,
                                          rtt_ms)) {
    rtc::CritScope cs(&observer_lock_);
    if (observer_) {
      observer_->OnNetworkChanged(target_bitrate_bps, fraction_loss, rtt_ms,
                                  probing_interval_ms);
    }
  }
}

bool EncodingRateController::Impl::HasNetworkParametersToReportChanged(
    int64_t target_bitrate_bps,
    uint8_t fraction_loss,
    int64_t rtt_ms) {
  bool changed = last_reported_target_bitrate_bps_ != target_bitrate_bps ||
                 (target_bitrate_bps > 0 &&
                  (last_reported_fraction_loss_ != fraction_loss ||
                   last_reported_rtt_ms_ != rtt_ms));
  if (changed &&
      (last_reported_target_bitrate_bps_ == 0 || target_bitrate_bps == 0)) {
    RTC_LOG(LS_INFO) << "Bitrate estimate state changed, BWE: "
                     << target_bitrate_bps << " bps.";
  }
  last_reported_target_bitrate_bps_ = target_bitrate_bps;
  last_reported_fraction_loss_ = fraction_loss;
  last_reported_rtt_ms_ = rtt_ms;
  return changed;
}

bool EncodingRateController::Impl::IsSendQueueFull() const {
  return pacer_expected_queue_ms_ > PacedSender::kMaxQueueLengthMs;
}
}  // namespace network
}  // namespace webrtc
