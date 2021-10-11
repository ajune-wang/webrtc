/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/gain_controller2.h"

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

constexpr int kUnspecifiedAnalogLevel = -1;
constexpr int kLogLimiterStatsPeriodMs = 30'000;
constexpr int kFrameLengthMs = 10;
constexpr int kLogLimiterStatsPeriodNumFrames =
    kLogLimiterStatsPeriodMs / kFrameLengthMs;

// Detects the available CPU features and applies any kill-switches.
AvailableCpuFeatures GetAllowedCpuFeatures() {
  AvailableCpuFeatures features = GetAvailableCpuFeatures();
  if (field_trial::IsEnabled("WebRTC-Agc2SimdSse2KillSwitch")) {
    features.sse2 = false;
  }
  if (field_trial::IsEnabled("WebRTC-Agc2SimdAvx2KillSwitch")) {
    features.avx2 = false;
  }
  if (field_trial::IsEnabled("WebRTC-Agc2SimdNeonKillSwitch")) {
    features.neon = false;
  }
  return features;
}

}  // namespace

int GainController2::instance_count_ = 0;

GainController2::GainController2(
    const AudioProcessing::Config::GainController2& config)
    : cpu_features_(GetAllowedCpuFeatures()),
      initialized_(false),
      data_dumper_(rtc::AtomicOps::Increment(&instance_count_)),
      fixed_gain_applier_(/*hard_clip_samples=*/false,
                          /*initial_gain_factor=*/0.0f),
      limiter_(/*sample_rate_hz=*/48000,
               &data_dumper_,
               /*histogram_name_prefix=*/"Agc2"),
      calls_since_last_limiter_log_(0),
      analog_level_(kUnspecifiedAnalogLevel) {
  RTC_DCHECK(Validate(config));
  fixed_gain_applier_.SetGainFactor(DbToRatio(config.fixed_digital.gain_db));
  const bool use_vad = config.adaptive_digital.enabled;
  if (use_vad) {
    // TODO(bugs.webrtc.org/7494): Move `vad_reset_period_ms` from adaptive
    // digital to gain controller 2 config.
    vad_ = std::make_unique<VoiceActivityDetectorWrapper>(
        config.adaptive_digital.vad_reset_period_ms, cpu_features_);
  }
  if (config.adaptive_digital.enabled) {
    RTC_DCHECK(use_vad);
    adaptive_digital_controller_ =
        std::make_unique<AdaptiveAgc>(&data_dumper_, config.adaptive_digital);
  }
}

GainController2::~GainController2() = default;

void GainController2::Initialize(int sample_rate_hz, int num_channels) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);
  limiter_.SetSampleRate(sample_rate_hz);
  if (vad_) {
    vad_->Initialize(sample_rate_hz);
  }
  if (adaptive_digital_controller_) {
    adaptive_digital_controller_->Initialize(sample_rate_hz, num_channels);
  }
  data_dumper_.InitiateNewSetOfRecordings();
  data_dumper_.DumpRaw("sample_rate_hz", sample_rate_hz);
  calls_since_last_limiter_log_ = 0;
  initialized_ = true;
}

void GainController2::SetFixedGainDb(float gain_db) {
  const float gain = DbToRatio(gain_db);
  // If the gain has changed, reset the limiter to quickly react on abrupt level
  // changes caused by the fixed gain change.
  constexpr float kEpsilon = 0.01f;
  if (std::fabs(gain - fixed_gain_applier_.GetGainFactor()) > kEpsilon) {
    limiter_.Reset();
  }
  fixed_gain_applier_.SetGainFactor(gain);
}

void GainController2::Process(AudioBuffer* audio) {
  RTC_DCHECK(initialized_);
  data_dumper_.DumpRaw("agc2_notified_analog_level", analog_level_);
  AudioFrameView<float> float_frame(audio->channels(), audio->num_channels(),
                                    audio->num_frames());
  absl::optional<float> speech_probability;
  if (vad_) {
    speech_probability = vad_->Analyze(float_frame);
    data_dumper_.DumpRaw("agc2_speech_probability", speech_probability.value());
  }
  fixed_gain_applier_.ApplyGain(float_frame);
  if (adaptive_digital_controller_) {
    RTC_DCHECK(speech_probability.has_value());
    adaptive_digital_controller_->Process(
        float_frame, speech_probability.value(), limiter_.LastAudioLevel());
  }
  limiter_.Process(float_frame);

  // Periodically log limiter stats.
  if (++calls_since_last_limiter_log_ == kLogLimiterStatsPeriodNumFrames) {
    calls_since_last_limiter_log_ = 0;
    InterpolatedGainCurve::Stats stats = limiter_.GetGainCurveStats();
    RTC_LOG(LS_INFO) << "AGC2 limiter stats"
                     << " | identity: " << stats.look_ups_identity_region
                     << " | knee: " << stats.look_ups_knee_region
                     << " | limiter: " << stats.look_ups_limiter_region
                     << " | saturation: " << stats.look_ups_saturation_region;
  }
}

void GainController2::NotifyAnalogLevel(int level) {
  if (analog_level_ != level && adaptive_digital_controller_) {
    adaptive_digital_controller_->HandleInputGainChange();
  }
  analog_level_ = level;
}

bool GainController2::Validate(
    const AudioProcessing::Config::GainController2& config) {
  const auto& fixed = config.fixed_digital;
  const auto& adaptive = config.adaptive_digital;
  return fixed.gain_db >= 0.0f && fixed.gain_db < 50.f &&
         adaptive.headroom_db >= 0.0f && adaptive.max_gain_db > 0.0f &&
         adaptive.initial_gain_db >= 0.0f &&
         adaptive.max_gain_change_db_per_second > 0.0f &&
         adaptive.max_output_noise_level_dbfs <= 0.0f;
}

}  // namespace webrtc
