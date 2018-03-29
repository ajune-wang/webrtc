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

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {
void ShiftBuffer(std::array<float, kPeakEnveloperBufferSize>* buffer_) {
  // Move everything one element back
  std::copy(buffer_->begin() + 1, buffer_->end(), buffer_->begin());
}

}  // namespace

SaturationProtector::PeakEnveloper::PeakEnveloper() = default;

// TODO(aleloi): rewrite the logic, it's messy and bug-prone! Find way to
// modularize.
void SaturationProtector::PeakEnveloper::Process(float frame_peak_dbfs) {
  /* Idea: produces delayed peak envelope, but handles the beginning
     differently.

     The delay is kPeakEnveloperBufferSize * kPeakEnveloperSuperFrameLengthMs.

     Every kFrameDurationMs, you feed the latest peak. In the
     beginning, the enveloper just returns the highest peak so far. It
     slowly builds up a buffer of the highest peaks in a super
     frame. After a while, it has enough history to return a
     superframe peak with correct delay. Until then, it returns the
     highest peak seen so far.

     We could simplify the code by dividing the query from the
     update. We could simplify more by not caring what happens during
     the first superframe. Instead we can accept a lower gain, or rely
     on the initial value in the SaturationProtector. In that case, we
     shouldn't update during the first superframe.

     The shifting is looks inefficient. It's not, because the buffer is
     very short. It should be < 10 values.
   */

  current_superframe_peak_dbfs_ =
      std::max(current_superframe_peak_dbfs_, frame_peak_dbfs);
  speech_time_in_estimate_ms_ += kFrameDurationMs;
  if (speech_time_in_estimate_ms_ > kPeakEnveloperSuperFrameLengthMs) {
    speech_time_in_estimate_ms_ = 0;
    const bool buffer_full = elements_in_buffer_ == kPeakEnveloperBufferSize;
    if (buffer_full) {
      ShiftBuffer(&peak_delay_buffer_);
      *peak_delay_buffer_.rbegin() = current_superframe_peak_dbfs_;
    } else {
      peak_delay_buffer_[elements_in_buffer_] = current_superframe_peak_dbfs_;
      elements_in_buffer_++;
    }
    current_superframe_peak_dbfs_ = -90.f;
  }
}

float SaturationProtector::PeakEnveloper::Query() const {
  float result;
  if (elements_in_buffer_ > 0) {
    result = peak_delay_buffer_[0];
  } else {
    result = current_superframe_peak_dbfs_;
  }
  return result;
}

SaturationProtector::SaturationProtector(ApmDataDumper* apm_data_dumper)
    : apm_data_dumper_(apm_data_dumper) {}

void SaturationProtector::UpdateMargin(
    const VadWithLevel::LevelAndProbability& vad_data,
    float last_speech_level_estimate) {
  peak_enveloper_.Process(vad_data.speech_peak_dbfs);
  const float delayed_peak_dbfs = peak_enveloper_.Query();
  const float difference_db = delayed_peak_dbfs - last_speech_level_estimate;

  if (last_margin_ < difference_db) {
    last_margin_ = last_margin_ * kSaturationProtectorAttackConstant +
                   difference_db * (1 - kSaturationProtectorAttackConstant);
  } else {
    last_margin_ = last_margin_ * kSaturationProtectorDecayConstant +
                   difference_db * (1 - kSaturationProtectorDecayConstant);
  }

  last_margin_ = rtc::SafeClamp<float>(last_margin_, 12.f, 25.f);
}

float SaturationProtector::LastMargin() const {
  return last_margin_;
}

void SaturationProtector::DebugDumpEstimate() const {
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_saturation_protector_delayed_peak_dbfs",
      peak_enveloper_.Query());
  apm_data_dumper_->DumpRaw("agc2_adaptive_saturation_margin_db", last_margin_);
}

}  // namespace webrtc
