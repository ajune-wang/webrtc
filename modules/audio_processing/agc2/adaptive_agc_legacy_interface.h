/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_AGC_LEGACY_INTERFACE_H_
#define MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_AGC_LEGACY_INTERFACE_H_

#include <memory>

#include "modules/audio_processing/agc/agc.h"
#include "modules/audio_processing/agc2/adaptive_agc.h"
// #include "modules/audio_processing/agc2/fixed_gain_controller.h"
// #include "modules/audio_processing/agc2/vad_with_level.h"
// #include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {
class ApmDataDumper;

class AdaptiveAgcLegacyInterface : public GainControl {
 public:
  explicit AdaptiveAgcLegacyInterface(ApmDataDumper* apm_data_dumper);
  // void Process(AudioFrameView<float> float_frame);
  ~AdaptiveAgcLegacyInterface() override;

  int Enable(bool enable) override;
  bool is_enabled() const override;

  // When an analog mode is set, this must be called prior to |ProcessStream()|
  // to pass the current analog level from the audio HAL. Must be within the
  // range provided to |set_analog_level_limits()|.
  int set_stream_analog_level(int level) override;

  // When an analog mode is set, this should be called after |ProcessStream()|
  // to obtain the recommended new analog level for the audio HAL. It is the
  // users responsibility to apply this level.
  int stream_analog_level() override;

  int set_mode(Mode mode) override;
  Mode mode() const override;

  // Sets the target peak |level| (or envelope) of the AGC in dBFs (decibels
  // from digital full-scale). The convention is to use positive values. For
  // instance, passing in a value of 3 corresponds to -3 dBFs, or a target
  // level 3 dB below full-scale. Limited to [0, 31].
  //
  // TODO(ajm): use a negative value here instead, if/when VoE will similarly
  //            update its interface.
  int set_target_level_dbfs(int level) override;
  int target_level_dbfs() const override;

  // Sets the maximum |gain| the digital compression stage may apply, in dB. A
  // higher number corresponds to greater compression, while a value of 0 will
  // leave the signal uncompressed. Limited to [0, 90].
  int set_compression_gain_db(int gain) override;
  int compression_gain_db() const override;

  // When enabled, the compression stage will hard limit the signal to the
  // target level. Otherwise, the signal will be compressed but not limited
  // above the target level.
  int enable_limiter(bool enable) override;
  bool is_limiter_enabled() const override;

  // Sets the |minimum| and |maximum| analog levels of the audio capture device.
  // Must be set if and only if an analog mode is used. Limited to [0, 65535].
  int set_analog_level_limits(int minimum, int maximum) override;
  int analog_level_minimum() const override;
  int analog_level_maximum() const override;

  // Returns true if the AGC has detected a saturation event (period where the
  // signal reaches digital full-scale) in the current frame and the analog
  // level cannot be reduced.
  //
  // This could be used as an indicator to reduce or disable analog mic gain at
  // the audio HAL.
  bool stream_is_saturated() const override;

  /*
    // These are from the Agc interface. We're maybe not going to use them.


  // Returns the proportion of samples in the buffer which are at full-scale
  // (and presumably clipped).
  float AnalyzePreproc(const int16_t* audio, size_t length) override;
  // |audio| must be mono; in a multi-channel stream, provide the first (usually
  // left) channel.
  void Process(const int16_t* audio,
               size_t length,
               int sample_rate_hz) override;

  // Retrieves the difference between the target RMS level and the current
  // signal RMS level in dB. Returns true if an update is available and false
  // otherwise, in which case |error| should be ignored and no action taken.
  bool GetRmsErrorDb(int* error) override;
  void Reset() override;

  // int set_target_level_dbfs(int level);
  // int target_level_dbfs() const;
  float voice_probability() const override;

  */

 private:
  AdaptiveAgc* agc_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_AGC_LEGACY_INTERFACE_H_
