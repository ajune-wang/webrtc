/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/apm_helpers.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace apm_helpers {

void Init(AudioProcessing* apm) {
  RTC_DCHECK(apm);

  constexpr int kMinVolumeLevel = 0;
  constexpr int kMaxVolumeLevel = 255;

  AudioProcessing::Config config = apm->GetConfig();
#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  config.gain_controller1.mode = config.gain_controller1.kFixedDigital;
#else
  config.gain_controller1.mode = config.gain_controller1.kAdaptiveAnalog;
#endif
  RTC_LOG(LS_INFO) << "Setting AGC mode to " << config.gain_controller1.mode;
  // This is the initialization which used to happen in VoEBase::Init(), but
  // which is not covered by the WVoE::ApplyOptions().
  config.gain_controller1.analog_level_minimum = kMinVolumeLevel;
  config.gain_controller1.analog_level_maximum = kMaxVolumeLevel;
  apm->ApplyConfig(config);
}

void UpdateAgcConfig(const cricket::AudioOptions& options,
                     AudioProcessing::Config::GainController1* config) {
  RTC_DCHECK(config);
  if (options.auto_gain_control) {
    const bool enabled = *options.auto_gain_control;
    config->enabled = enabled;
    RTC_LOG(LS_INFO) << "Setting AGC to " << enabled;
  }
  if (options.tx_agc_target_dbov) {
    config->target_level_dbfs = *options.tx_agc_target_dbov;
  }
  if (options.tx_agc_digital_compression_gain) {
    config->compression_gain_db = *options.tx_agc_digital_compression_gain;
  }
  if (options.tx_agc_limiter) {
    config->enable_limiter = *options.tx_agc_limiter;
  }
}

void SetEcStatus(AudioProcessing* apm, bool enable, EcModes mode) {
  RTC_DCHECK(apm);
  RTC_DCHECK(mode == kEcConference || mode == kEcAecm) << "mode: " << mode;
  AudioProcessing::Config apm_config = apm->GetConfig();
  apm_config.echo_canceller.enabled = enable;
  apm_config.echo_canceller.mobile_mode = (mode == kEcAecm);
  apm_config.echo_canceller.legacy_moderate_suppression_level = false;
  apm->ApplyConfig(apm_config);
  RTC_LOG(LS_INFO) << "Echo control set to " << enable << " with mode " << mode;
}

void SetNsStatus(AudioProcessing* apm, bool enable) {
  RTC_DCHECK(apm);
  NoiseSuppression* ns = apm->noise_suppression();
  if (ns->set_level(NoiseSuppression::kHigh) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to set high NS level.";
    return;
  }
  if (ns->Enable(enable) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to enable/disable NS: " << enable;
    return;
  }
  RTC_LOG(LS_INFO) << "NS set to " << enable;
}
}  // namespace apm_helpers
}  // namespace webrtc
