/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector.h"

#include <algorithm>
#include <iterator>

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

constexpr float kMinLevelDbfs = -90.f;

// Min/max margins are based on speech crest-factor.
constexpr float kMinMarginDb = 12.f;
constexpr float kMaxMarginDb = 25.f;

constexpr int kRingBuffCapacity = static_cast<int>(kPeakEnveloperBufferSize);

}  // namespace

SaturationProtector::SaturationProtector(ApmDataDumper* apm_data_dumper)
    : SaturationProtector(apm_data_dumper,
                          GetInitialSaturationMarginDb(),
                          GetExtraSaturationMarginOffsetDb()) {}

SaturationProtector::SaturationProtector(ApmDataDumper* apm_data_dumper,
                                         float initial_saturation_margin_db,
                                         float extra_saturation_margin_db)
    : apm_data_dumper_(apm_data_dumper),
      initial_saturation_margin_db_(initial_saturation_margin_db),
      extra_saturation_margin_db_(extra_saturation_margin_db) {
  Reset();
}

void SaturationProtector::Reset() {
  margin_db_ = initial_saturation_margin_db_;
  peak_delay_buffer_.next = 0;
  peak_delay_buffer_.size = 0;
  max_peaks_dbfs_ = kMinLevelDbfs;
  time_since_push_ms_ = 0;
}

void SaturationProtector::UpdateMargin(float speech_peak_dbfs,
                                       float speech_level_dbfs) {
  RTC_DCHECK_EQ(static_cast<int>(peak_delay_buffer_.buffer.size()),
                kRingBuffCapacity);
  RTC_DCHECK_LT(peak_delay_buffer_.next, kRingBuffCapacity);
  RTC_DCHECK_LE(peak_delay_buffer_.size, kRingBuffCapacity);

  // Get the max peak over `kPeakEnveloperSuperFrameLengthMs` ms.
  max_peaks_dbfs_ = std::max(max_peaks_dbfs_, speech_peak_dbfs);
  time_since_push_ms_ += kFrameDurationMs;
  if (time_since_push_ms_ >
      static_cast<int>(kPeakEnveloperSuperFrameLengthMs)) {
    // Push `max_peaks_dbfs_` back into the ring buffer.
    peak_delay_buffer_.buffer[peak_delay_buffer_.next++] = max_peaks_dbfs_;
    if (peak_delay_buffer_.next == kRingBuffCapacity) {
      peak_delay_buffer_.next = 0;
    }
    if (peak_delay_buffer_.size < kRingBuffCapacity) {
      peak_delay_buffer_.size++;
    }
    // Reset.
    time_since_push_ms_ = 0;
    max_peaks_dbfs_ = kMinLevelDbfs;
  }

  // Update margin by comparing the estimated speech level and the delayed max
  // speech peak power.
  const float difference_db = GetDelayedPeakDbfs() - speech_level_dbfs;
  if (margin_db_ < difference_db) {
    margin_db_ = margin_db_ * kSaturationProtectorAttackConstant +
                 difference_db * (1.f - kSaturationProtectorAttackConstant);
  } else {
    margin_db_ = margin_db_ * kSaturationProtectorDecayConstant +
                 difference_db * (1.f - kSaturationProtectorDecayConstant);
  }

  margin_db_ = rtc::SafeClamp<float>(margin_db_, kMinMarginDb, kMaxMarginDb);
}

float SaturationProtector::GetDelayedPeakDbfs() const {
  // TODO(alessiob): Check with aleloi@ why we use a delay and how to tune it.
  return peak_delay_buffer_.size == 0
             ? max_peaks_dbfs_
             : peak_delay_buffer_
                   .buffer[peak_delay_buffer_.size == kRingBuffCapacity
                               ? peak_delay_buffer_.next
                               : 0];
}

float SaturationProtector::LastMargin() const {
  return margin_db_ + extra_saturation_margin_db_;
}

void SaturationProtector::DebugDumpEstimate() const {
  if (apm_data_dumper_) {
    apm_data_dumper_->DumpRaw(
        "agc2_adaptive_saturation_protector_delayed_peak_dbfs",
        GetDelayedPeakDbfs());
    apm_data_dumper_->DumpRaw("agc2_adaptive_saturation_margin_db", margin_db_);
  }
}

}  // namespace webrtc
