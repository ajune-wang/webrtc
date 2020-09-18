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

#include <iostream>

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

// Min/max margins are based on speech crest-factor.
constexpr float kMinMarginDb = 12.f;
constexpr float kMaxMarginDb = 25.f;

constexpr int kRingBuffCapacity = static_cast<int>(kPeakEnveloperBufferSize);

}  // namespace

void ResetSaturationProtectorState(float initial_margin_db,
                                   SaturationProtectorState& state) {
  state.margin_db = initial_margin_db;
  state.speech_peaks_dbfs.next = 0;
  state.speech_peaks_dbfs.size = 0;
  state.max_peaks_dbfs = kMinLevelDbfs;
  state.time_since_push_ms = 0;
}

bool SaturationProtectorState::operator==(
    const SaturationProtectorState& b) const {
  if (margin_db != b.margin_db ||
      speech_peaks_dbfs.buffer.size() != b.speech_peaks_dbfs.buffer.size() ||
      speech_peaks_dbfs.next != b.speech_peaks_dbfs.next ||
      speech_peaks_dbfs.size != b.speech_peaks_dbfs.size ||
      time_since_push_ms != b.time_since_push_ms ||
      max_peaks_dbfs != b.max_peaks_dbfs) {
    return false;
  }
  RTC_DCHECK_EQ(static_cast<int>(speech_peaks_dbfs.buffer.size()),
                kRingBuffCapacity);
  // No need to take into account `speech_peaks_dbfs.next` since
  // - if the buffer is full, we must compare all the pairs
  // - otherwise, only the indexes in [0, size) are used.
  for (int i = 0; i < speech_peaks_dbfs.size; ++i) {
    RTC_DCHECK_LT(i, kRingBuffCapacity);
    if (speech_peaks_dbfs.buffer[i] != b.speech_peaks_dbfs.buffer[i]) {
      return false;
    }
  }
  return true;
}

bool SaturationProtectorState::operator!=(
    const SaturationProtectorState& s) const {
  return !(*this == s);
}

void UpdateSaturationProtectorState(float speech_level_dbfs,
                                    float speech_peak_dbfs,
                                    SaturationProtectorState& state) {
  auto& ring_buffer = state.speech_peaks_dbfs.buffer;
  auto& next = state.speech_peaks_dbfs.next;
  auto& size = state.speech_peaks_dbfs.size;
  RTC_DCHECK_EQ(static_cast<int>(ring_buffer.size()), kRingBuffCapacity);
  RTC_DCHECK_LT(next, kRingBuffCapacity);
  RTC_DCHECK_LE(size, kRingBuffCapacity);

  // Get the max peak over `kPeakEnveloperSuperFrameLengthMs` ms.
  state.max_peaks_dbfs = std::max(state.max_peaks_dbfs, speech_peak_dbfs);
  state.time_since_push_ms += kFrameDurationMs;
  if (state.time_since_push_ms >
      static_cast<int>(kPeakEnveloperSuperFrameLengthMs)) {
    // Push `max_peaks_dbfs` back into the ring buffer.
    ring_buffer[next++] = state.max_peaks_dbfs;
    if (next == kRingBuffCapacity) {
      next = 0;
    }
    if (size < kRingBuffCapacity) {
      size++;
    }
    // Reset.
    state.time_since_push_ms = 0;
    state.max_peaks_dbfs = kMinLevelDbfs;
  }

  // Update margin by comparing the estimated speech level and the delayed max
  // speech peak power.
  // TODO(alessiob): Check with aleloi@ why we use a delay and how to tune it.
  const float delayed_peak_dbfs =
      size == 0 ? state.max_peaks_dbfs
                : ring_buffer[size == kRingBuffCapacity ? next : 0];
  const float difference_db = delayed_peak_dbfs - speech_level_dbfs;
  if (difference_db > state.margin_db) {
    // Attack.
    state.margin_db =
        state.margin_db * kSaturationProtectorAttackConstant +
        difference_db * (1.f - kSaturationProtectorAttackConstant);
  } else {
    // Decay.
    state.margin_db = state.margin_db * kSaturationProtectorDecayConstant +
                      difference_db * (1.f - kSaturationProtectorDecayConstant);
  }
  state.margin_db =
      rtc::SafeClamp<float>(state.margin_db, kMinMarginDb, kMaxMarginDb);
}

}  // namespace webrtc
