/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"

#include <assert.h>

#include <cstdint>
#include <utility>

#include "absl/types/optional.h"
#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/inter_arrival.h"
#include "modules/remote_bitrate_estimator/overuse_detector.h"
#include "modules/remote_bitrate_estimator/overuse_estimator.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
absl::optional<DataRate> OptionalRateFromOptionalBps(
    absl::optional<int> bitrate_bps) {
  if (bitrate_bps) {
    return DataRate::BitsPerSec(*bitrate_bps);
  } else {
    return absl::nullopt;
  }
}

constexpr TimeDelta kProcessInterval = TimeDelta::Millis(500);
constexpr int kTimestampGroupLengthMs = 5;
constexpr double kTimestampToMs = 1.0 / 90.0;

}  // namespace

struct RemoteBitrateEstimatorSingleStream::Detector {
  explicit Detector(int64_t last_packet_time_ms,
                    const OverUseDetectorOptions& options,
                    bool enable_burst_grouping,
                    const WebRtcKeyValueConfig* key_value_config)
      : last_packet_time_ms(last_packet_time_ms),
        inter_arrival(90 * kTimestampGroupLengthMs,
                      kTimestampToMs,
                      enable_burst_grouping),
        estimator(options),
        detector(key_value_config) {}
  int64_t last_packet_time_ms;
  InterArrival inter_arrival;
  OveruseEstimator estimator;
  OveruseDetector detector;
};

RemoteBitrateEstimatorSingleStream::RemoteBitrateEstimatorSingleStream(
    RemoteBitrateObserver* observer,
    Clock* clock)
    : clock_(clock),
      incoming_bitrate_(kBitrateWindowMs, 8000),
      last_valid_incoming_bitrate_(0),
      remote_rate_(field_trials_),
      observer_(observer),
      last_process_time_(Timestamp::MinusInfinity()),
      process_interval_(kProcessInterval),
      uma_recorded_(false) {
  RTC_LOG(LS_INFO) << "RemoteBitrateEstimatorSingleStream: Instantiating.";
}

RemoteBitrateEstimatorSingleStream::~RemoteBitrateEstimatorSingleStream() =
    default;

void RemoteBitrateEstimatorSingleStream::IncomingPacket(
    int64_t arrival_time_ms,
    size_t payload_size,
    const RTPHeader& header) {
  if (!uma_recorded_) {
    BweNames type = BweNames::kReceiverTOffset;
    if (!header.extension.hasTransmissionTimeOffset)
      type = BweNames::kReceiverNoExtension;
    RTC_HISTOGRAM_ENUMERATION(kBweTypeHistogram, type, BweNames::kBweNamesMax);
    uma_recorded_ = true;
  }
  uint32_t ssrc = header.ssrc;
  uint32_t rtp_timestamp =
      header.timestamp + header.extension.transmissionTimeOffset;
  int64_t now_ms = clock_->TimeInMilliseconds();
  rtc::CritScope cs(&crit_sect_);
  std::unique_ptr<Detector>& detector_ptr = overuse_detectors_[ssrc];
  if (detector_ptr == nullptr) {
    detector_ptr = std::make_unique<Detector>(now_ms, OverUseDetectorOptions(),
                                              true, &field_trials_);
  }
  Detector* estimator = detector_ptr.get();
  estimator->last_packet_time_ms = now_ms;

  // Check if incoming bitrate estimate is valid, and if it needs to be reset.
  absl::optional<uint32_t> incoming_bitrate = incoming_bitrate_.Rate(now_ms);
  if (incoming_bitrate) {
    last_valid_incoming_bitrate_ = *incoming_bitrate;
  } else if (last_valid_incoming_bitrate_ > 0) {
    // Incoming bitrate had a previous valid value, but now not enough data
    // point are left within the current window. Reset incoming bitrate
    // estimator so that the window size will only contain new data points.
    incoming_bitrate_.Reset();
    last_valid_incoming_bitrate_ = 0;
  }
  incoming_bitrate_.Update(payload_size, now_ms);

  const BandwidthUsage prior_state = estimator->detector.State();
  uint32_t timestamp_delta = 0;
  int64_t time_delta = 0;
  int size_delta = 0;
  if (estimator->inter_arrival.ComputeDeltas(
          rtp_timestamp, arrival_time_ms, now_ms, payload_size,
          &timestamp_delta, &time_delta, &size_delta)) {
    double timestamp_delta_ms = timestamp_delta * kTimestampToMs;
    estimator->estimator.Update(time_delta, timestamp_delta_ms, size_delta,
                                estimator->detector.State(), now_ms);
    estimator->detector.Detect(estimator->estimator.offset(),
                               timestamp_delta_ms,
                               estimator->estimator.num_of_deltas(), now_ms);
  }
  if (estimator->detector.State() == BandwidthUsage::kBwOverusing) {
    absl::optional<uint32_t> incoming_bitrate_bps =
        incoming_bitrate_.Rate(now_ms);
    if (incoming_bitrate_bps &&
        (prior_state != BandwidthUsage::kBwOverusing ||
         remote_rate_.TimeToReduceFurther(
             Timestamp::Millis(now_ms),
             DataRate::BitsPerSec(*incoming_bitrate_bps)))) {
      // The first overuse should immediately trigger a new estimate.
      // We also have to update the estimate immediately if we are overusing
      // and the target bitrate is too high compared to what we are receiving.
      UpdateEstimate(now_ms);
    }
  }
}

void RemoteBitrateEstimatorSingleStream::Process() {
  {
    rtc::CritScope cs(&crit_sect_);
    UpdateEstimate(clock_->TimeInMilliseconds());
  }
  last_process_time_ = clock_->CurrentTime();
}

int64_t RemoteBitrateEstimatorSingleStream::TimeUntilNextProcess() {
  if (!last_process_time_.IsFinite()) {
    return 0;
  }
  rtc::CritScope cs_(&crit_sect_);
  RTC_DCHECK_GT(process_interval_, TimeDelta::Zero());
  return (last_process_time_ + process_interval_ - clock_->CurrentTime()).ms();
}

void RemoteBitrateEstimatorSingleStream::UpdateEstimate(int64_t now_ms) {
  BandwidthUsage bw_state = BandwidthUsage::kBwNormal;
  auto it = overuse_detectors_.begin();
  while (it != overuse_detectors_.end()) {
    const int64_t time_of_last_received_packet =
        it->second->last_packet_time_ms;
    if (time_of_last_received_packet >= 0 &&
        now_ms - time_of_last_received_packet > kStreamTimeOutMs) {
      // This over-use detector hasn't received packets for |kStreamTimeOutMs|
      // milliseconds and is considered stale.
      overuse_detectors_.erase(it++);
    } else {
      // Make sure that we trigger an over-use if any of the over-use detectors
      // is detecting over-use.
      if (it->second->detector.State() > bw_state) {
        bw_state = it->second->detector.State();
      }
      ++it;
    }
  }
  // We can't update the estimate if we don't have any active streams.
  if (overuse_detectors_.empty()) {
    return;
  }

  const RateControlInput input(
      bw_state, OptionalRateFromOptionalBps(incoming_bitrate_.Rate(now_ms)));
  uint32_t target_bitrate =
      remote_rate_.Update(&input, Timestamp::Millis(now_ms)).bps<uint32_t>();
  if (remote_rate_.ValidEstimate()) {
    process_interval_ = remote_rate_.GetFeedbackInterval();
    RTC_DCHECK_GT(process_interval_, TimeDelta::Zero());
    if (observer_)
      observer_->OnReceiveBitrateChanged(GetSsrcs(), target_bitrate);
  }
}

void RemoteBitrateEstimatorSingleStream::OnRttUpdate(int64_t avg_rtt_ms,
                                                     int64_t max_rtt_ms) {
  rtc::CritScope cs(&crit_sect_);
  remote_rate_.SetRtt(TimeDelta::Millis(avg_rtt_ms));
}

void RemoteBitrateEstimatorSingleStream::RemoveStream(unsigned int ssrc) {
  rtc::CritScope cs(&crit_sect_);
  overuse_detectors_.erase(ssrc);
}

bool RemoteBitrateEstimatorSingleStream::LatestEstimate(
    std::vector<uint32_t>* ssrcs,
    uint32_t* bitrate_bps) const {
  rtc::CritScope cs(&crit_sect_);
  RTC_DCHECK(ssrcs);
  RTC_DCHECK(bitrate_bps);
  if (!remote_rate_.ValidEstimate()) {
    return false;
  }
  *ssrcs = GetSsrcs();
  if (ssrcs->empty())
    *bitrate_bps = 0;
  else
    *bitrate_bps = remote_rate_.LatestEstimate().bps<uint32_t>();
  return true;
}

std::vector<uint32_t> RemoteBitrateEstimatorSingleStream::GetSsrcs() const {
  std::vector<uint32_t> ssrcs;
  ssrcs.reserve(overuse_detectors_.size());
  for (const auto& kv : overuse_detectors_) {
    ssrcs.push_back(kv.first);
  }
  return ssrcs;
}

void RemoteBitrateEstimatorSingleStream::SetMinBitrate(int min_bitrate_bps) {
  rtc::CritScope cs(&crit_sect_);
  remote_rate_.SetMinBitrate(DataRate::BitsPerSec(min_bitrate_bps));
}

}  // namespace webrtc
