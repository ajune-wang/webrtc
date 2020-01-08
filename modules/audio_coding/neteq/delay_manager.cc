/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/delay_manager.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>

#include "modules/audio_coding/neteq/histogram.h"
#include "modules/include/module_common_types_public.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"

namespace {

// Arbitrary number that is only used until the second packet is received.
constexpr int kStartDelayMs = 80;

constexpr int kMinBaseMinimumDelayMs = 0;
constexpr int kMaxBaseMinimumDelayMs = 10000;
constexpr int kMaxReorderedPackets =
    10;  // Max number of consecutive reordered packets.
constexpr int kMaxHistoryMs = 2000;  // Oldest packet to include in history to
                                     // calculate relative packet arrival delay.
constexpr int kDelayBuckets = 100;
constexpr int kBucketSizeMs = 20;

int PercentileToQuantile(double percentile) {
  return static_cast<int>((1 << 30) * percentile / 100.0 + 0.5);
}

struct DelayHistogramConfig {
  int quantile = 1041529569;  // 0.97 in Q30.
  int forget_factor = 32745;  // 0.9993 in Q15.
  absl::optional<double> start_forget_weight = 2;
};

DelayHistogramConfig GetDelayHistogramConfig() {
  constexpr char kDelayHistogramFieldTrial[] =
      "WebRTC-Audio-NetEqDelayHistogram";
  DelayHistogramConfig config;
  if (webrtc::field_trial::IsEnabled(kDelayHistogramFieldTrial)) {
    const auto field_trial_string =
        webrtc::field_trial::FindFullName(kDelayHistogramFieldTrial);
    double percentile = -1.0;
    double forget_factor = -1.0;
    double start_forget_weight = -1.0;
    if (sscanf(field_trial_string.c_str(), "Enabled-%lf-%lf-%lf", &percentile,
               &forget_factor, &start_forget_weight) >= 2 &&
        percentile >= 0.0 && percentile <= 100.0 && forget_factor >= 0.0 &&
        forget_factor <= 1.0) {
      config.quantile = PercentileToQuantile(percentile);
      config.forget_factor = (1 << 15) * forget_factor;
      config.start_forget_weight =
          start_forget_weight >= 1 ? absl::make_optional(start_forget_weight)
                                   : absl::nullopt;
    }
  }
  RTC_LOG(LS_INFO) << "Delay histogram config:"
                   << " quantile=" << config.quantile
                   << " forget_factor=" << config.forget_factor
                   << " start_forget_weight="
                   << config.start_forget_weight.value_or(0);
  return config;
}

}  // namespace

namespace webrtc {

DelayManager::DelayManager(size_t max_packets_in_buffer,
                           int base_minimum_delay_ms,
                           int histogram_quantile,
                           bool enable_rtx_handling,
                           const TickTimer* tick_timer,
                           std::unique_ptr<Histogram> histogram)
    : first_packet_received_(false),
      max_packets_in_buffer_(max_packets_in_buffer),
      histogram_(std::move(histogram)),
      histogram_quantile_(histogram_quantile),
      tick_timer_(tick_timer),
      base_minimum_delay_ms_(base_minimum_delay_ms),
      effective_minimum_delay_ms_(base_minimum_delay_ms),
      target_level_(kStartDelayMs),
      last_timestamp_(0),
      minimum_delay_ms_(0),
      maximum_delay_ms_(0),
      last_pack_cng_or_dtmf_(1),
      enable_rtx_handling_(enable_rtx_handling) {
  RTC_CHECK(histogram_);
  RTC_DCHECK_GE(base_minimum_delay_ms_, 0);

  Reset();
}

std::unique_ptr<DelayManager> DelayManager::Create(
    size_t max_packets_in_buffer,
    int base_minimum_delay_ms,
    bool enable_rtx_handling,
    const TickTimer* tick_timer) {
  DelayHistogramConfig config = GetDelayHistogramConfig();
  const int quantile = config.quantile;
  std::unique_ptr<Histogram> histogram = std::make_unique<Histogram>(
      kDelayBuckets, config.forget_factor, config.start_forget_weight);
  return std::make_unique<DelayManager>(
      max_packets_in_buffer, base_minimum_delay_ms, quantile,
      enable_rtx_handling, tick_timer, std::move(histogram));
}

DelayManager::~DelayManager() {}

absl::optional<int> DelayManager::Update(uint16_t sequence_number,
                                         uint32_t timestamp,
                                         int sample_rate_hz) {
  if (sample_rate_hz <= 0) {
    return absl::nullopt;
  }

  if (!first_packet_received_) {
    // Prepare for next packet arrival.
    packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
    last_timestamp_ = timestamp;
    first_packet_received_ = true;
    return absl::nullopt;
  }

  bool reordered = false;
  absl::optional<int> relative_delay;
  int iat_ms = packet_iat_stopwatch_->ElapsedMs();
  int ts_diff = timestamp - last_timestamp_;
  int expected_iat_ms = ts_diff / (sample_rate_hz / 1000);
  int iat_delay = iat_ms - expected_iat_ms;

  // Check for discontinuous packet sequence and re-ordering.
  if (IsNewerTimestamp(timestamp, last_timestamp_)) {
    UpdateDelayHistory(iat_delay, timestamp, sample_rate_hz);
    relative_delay = CalculateRelativePacketArrivalDelay();
  } else {
    relative_delay = std::max(iat_delay, 0);
    reordered = true;
  }

  const int index = relative_delay.value() / kBucketSizeMs;
  if (index < histogram_->NumBuckets()) {
    // Maximum delay to register is 2000 ms.
    histogram_->Add(index);
  }
  // Calculate new |target_level_| based on updated statistics.
  int bucket_index = histogram_->Quantile(histogram_quantile_);
  int target_level = (bucket_index + 1) * kBucketSizeMs;
  target_level_ = std::max(target_level, 1);
  LimitTargetLevel();

  if (enable_rtx_handling_ && reordered &&
      num_reordered_packets_ < kMaxReorderedPackets) {
    ++num_reordered_packets_;
    return relative_delay;
  }
  num_reordered_packets_ = 0;
  // Prepare for next packet arrival.
  packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
  last_timestamp_ = timestamp;
  return relative_delay;
}

void DelayManager::UpdateDelayHistory(int iat_delay_ms,
                                      uint32_t timestamp,
                                      int sample_rate_hz) {
  PacketDelay delay;
  delay.iat_delay_ms = iat_delay_ms;
  delay.timestamp = timestamp;
  delay_history_.push_back(delay);
  while (timestamp - delay_history_.front().timestamp >
         static_cast<uint32_t>(kMaxHistoryMs * sample_rate_hz / 1000)) {
    delay_history_.pop_front();
  }
}

int DelayManager::CalculateRelativePacketArrivalDelay() const {
  // This effectively calculates arrival delay of a packet relative to the
  // packet preceding the history window. If the arrival delay ever becomes
  // smaller than zero, it means the reference packet is invalid, and we
  // move the reference.
  int relative_delay = 0;
  for (const PacketDelay& delay : delay_history_) {
    relative_delay += delay.iat_delay_ms;
    relative_delay = std::max(relative_delay, 0);
  }
  return relative_delay;
}

// Enforces upper and lower limits for |target_level_|. The upper limit is
// chosen to be minimum of i) 75% of |max_packets_in_buffer_|, to leave some
// headroom for natural fluctuations around the target, and ii) equivalent of
// |maximum_delay_ms_| in packets. Note that in practice, if no
// |maximum_delay_ms_| is specified, this does not have any impact, since the
// target level is far below the buffer capacity in all reasonable cases.
// The lower limit is equivalent of |effective_minimum_delay_ms_| in packets.
// We update |least_required_level_| while the above limits are applied.
// TODO(hlundin): Move this check to the buffer logistics class.
void DelayManager::LimitTargetLevel() {
  if (effective_minimum_delay_ms_ > 0) {
    target_level_ = std::max(target_level_, effective_minimum_delay_ms_);
  }

  if (maximum_delay_ms_ > 0) {
    target_level_ = std::min(target_level_, maximum_delay_ms_);
  }

  // Sanity check, at least 1 packet (in Q8).
  target_level_ = std::max(target_level_, 0);
}

void DelayManager::Reset() {
  histogram_->Reset();
  delay_history_.clear();
  target_level_ = kStartDelayMs;
  packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
  last_pack_cng_or_dtmf_ = 1;
}

void DelayManager::ResetPacketIatCount() {
  packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
}

int DelayManager::TargetLevel() const {
  return target_level_;
}

void DelayManager::LastDecodedWasCngOrDtmf(bool it_was) {
  if (it_was) {
    last_pack_cng_or_dtmf_ = 1;
  } else if (last_pack_cng_or_dtmf_ != 0) {
    last_pack_cng_or_dtmf_ = -1;
  }
}

bool DelayManager::IsValidMinimumDelay(int delay_ms) const {
  return 0 <= delay_ms && delay_ms <= MinimumDelayUpperBound();
}

bool DelayManager::IsValidBaseMinimumDelay(int delay_ms) const {
  return kMinBaseMinimumDelayMs <= delay_ms &&
         delay_ms <= kMaxBaseMinimumDelayMs;
}

bool DelayManager::SetMinimumDelay(int delay_ms) {
  if (!IsValidMinimumDelay(delay_ms)) {
    return false;
  }

  minimum_delay_ms_ = delay_ms;
  UpdateEffectiveMinimumDelay();
  return true;
}

bool DelayManager::SetMaximumDelay(int delay_ms) {
  // If |delay_ms| is zero then it unsets the maximum delay and target level is
  // unconstrained by maximum delay.
  if (delay_ms != 0 && delay_ms < minimum_delay_ms_) {
    // Maximum delay shouldn't be less than minimum delay.
    return false;
  }

  maximum_delay_ms_ = delay_ms;
  UpdateEffectiveMinimumDelay();
  return true;
}

bool DelayManager::SetBaseMinimumDelay(int delay_ms) {
  if (!IsValidBaseMinimumDelay(delay_ms)) {
    return false;
  }

  base_minimum_delay_ms_ = delay_ms;
  UpdateEffectiveMinimumDelay();
  return true;
}

int DelayManager::GetBaseMinimumDelay() const {
  return base_minimum_delay_ms_;
}

int DelayManager::last_pack_cng_or_dtmf() const {
  return last_pack_cng_or_dtmf_;
}

void DelayManager::set_last_pack_cng_or_dtmf(int value) {
  last_pack_cng_or_dtmf_ = value;
}

void DelayManager::UpdateEffectiveMinimumDelay() {
  // Clamp |base_minimum_delay_ms_| into the range which can be effectively
  // used.
  const int base_minimum_delay_ms =
      rtc::SafeClamp(base_minimum_delay_ms_, 0, MinimumDelayUpperBound());
  effective_minimum_delay_ms_ =
      std::max(minimum_delay_ms_, base_minimum_delay_ms);
}

int DelayManager::MinimumDelayUpperBound() const {
  // Choose the lowest possible bound discarding 0 cases which mean the value
  // is not set and unconstrained.
  return maximum_delay_ms_ > 0 ? maximum_delay_ms_ : kMaxBaseMinimumDelayMs;
}

}  // namespace webrtc
