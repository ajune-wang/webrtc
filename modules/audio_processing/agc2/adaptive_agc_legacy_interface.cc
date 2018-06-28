/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_agc_legacy_interface.h"
namespace webrtc {

AdaptiveAgcLegacyInterface::~AdaptiveAgcLegacyInterface() = default;

int AdaptiveAgcLegacyInterface::Enable(bool enable) {
  RTC_NOTREACHED();
}
bool AdaptiveAgcLegacyInterface::is_enabled() const {
  RTC_NOTREACHED();
}

// When an analog mode is set, this must be called prior to |ProcessStream()|
// to pass the current analog level from the audio HAL. Must be within the
// range provided to |set_analog_level_limits()|.
int AdaptiveAgcLegacyInterface::set_stream_analog_level(int level) {
  RTC_NOTREACHED();
}

// When an analog mode is set, this should be called after |ProcessStream()|
// to obtain the recommended new analog level for the audio HAL. It is the
// users responsibility to apply this level.
int AdaptiveAgcLegacyInterface::stream_analog_level() {
  RTC_NOTREACHED();
}

int AdaptiveAgcLegacyInterface::set_mode(Mode mode) {
  RTC_NOTREACHED();
}
AdaptiveAgcLegacyInterface::Mode AdaptiveAgcLegacyInterface::mode() const {
  RTC_NOTREACHED();
}

// Sets the target peak |level| (or envelope) of the AGC in dBFs (decibels
// from digital full-scale). The convention is to use positive values. For
// instance, passing in a value of 3 corresponds to -3 dBFs, or a target
// level 3 dB below full-scale. Limited to [0, 31].
//
// TODO(ajm): use a negative value here instead, if/when VoE will similarly
//            update its interface.
int AdaptiveAgcLegacyInterface::set_target_level_dbfs(int level) {
  RTC_NOTREACHED();
}
int AdaptiveAgcLegacyInterface::target_level_dbfs() const {
  RTC_NOTREACHED();
}

// Sets the maximum |gain| the digital compression stage may apply, in dB. A
// higher number corresponds to greater compression, while a value of 0 will
// leave the signal uncompressed. Limited to [0, 90].
//
int AdaptiveAgcLegacyInterface::set_compression_gain_db(int gain) {
  //fixed_gain_controller_.SetGain(gain);
  return 0;
}
int AdaptiveAgcLegacyInterface::compression_gain_db() const {
  RTC_NOTREACHED();
}

// When enabled, the compression stage will hard limit the signal to the
// target level. Otherwise, the signal will be compressed but not limited
// above the target level.
int AdaptiveAgcLegacyInterface::enable_limiter(bool enable) {
  RTC_NOTREACHED();
}
bool AdaptiveAgcLegacyInterface::is_limiter_enabled() const {
  RTC_NOTREACHED();
}

// Sets the |minimum| and |maximum| analog levels of the audio capture device.
// Must be set if and only if an analog mode is used. Limited to [0, 65535].
int AdaptiveAgcLegacyInterface::set_analog_level_limits(int minimum,
                                                        int maximum) {
  RTC_NOTREACHED();
}
int AdaptiveAgcLegacyInterface::analog_level_minimum() const {
  RTC_NOTREACHED();
}
int AdaptiveAgcLegacyInterface::analog_level_maximum() const {
  RTC_NOTREACHED();
}

// Returns true if the AGC has detected a saturation event (period where the
// signal reaches digital full-scale) in the current frame and the analog
// level cannot be reduced.
//
// This could be used as an indicator to reduce or disable analog mic gain at
// the audio HAL.
bool AdaptiveAgcLegacyInterface::stream_is_saturated() const {
  RTC_NOTREACHED();
}
/*
// Returns the proportion of samples in the buffer which are at full-scale
// (and presumably clipped).
float AdaptiveAgcLegacyInterface::AnalyzePreproc(const int16_t* audio,
                                                 size_t length) {
  RTC_DCHECK_GT(length, 0);
  size_t num_clipped = 0;
  for (size_t i = 0; i < length; ++i) {
    if (audio[i] == 32767 || audio[i] == -32768)
      ++num_clipped;
  }
  return 1.0f * num_clipped / length;
}

// Feeds audio to the level estimator.
void AdaptiveAgcLegacyInterface::Process(const int16_t* audio,
                                         size_t length,
                                         int sample_rate_hz) {
  // TODO(aleloi): translate signal to floats. Send to the estimator.
  RTC_NOTREACHED();
}

// Retrieves the difference between the target RMS level and the current
// signal RMS level in dB. Returns true if an update is available and false
// otherwise, in which case |error| should be ignored and no action taken.
bool AdaptiveAgcLegacyInterface::GetRmsErrorDb(int* error) {
  RTC_NOTREACHED();
}

void AdaptiveAgcLegacyInterface::Reset() {
  RTC_NOTREACHED();
}

// int set_target_level_dbfs(int level);
// int target_level_dbfs() const;
float AdaptiveAgcLegacyInterface::voice_probability() const {
  RTC_NOTREACHED();
}

*/

}  // namespace webrtc
