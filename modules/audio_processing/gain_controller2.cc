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
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {

constexpr float kInitialFixedDigitalGain = 1.f;
constexpr bool kHardClipSamples = true;
constexpr bool kDoNotHardClipSamples = false;

}  // namespace

int GainController2::instance_count_ = 0;

GainController2::GainController2()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      pre_fixed_digital_gain_applier_(kHardClipSamples,
                                      kInitialFixedDigitalGain),
      fixed_digital_gain_applier_(kDoNotHardClipSamples,
                                  kInitialFixedDigitalGain),
      adaptive_digital_controller_(new AdaptiveAgc(data_dumper_.get())),
      limiter_(static_cast<size_t>(48000), data_dumper_.get(), "Agc2"),
      fixed_pregain_has_changed_(false) {}

GainController2::~GainController2() = default;

void GainController2::HandleCapturePreGainRuntimeSettings(float gain_factor) {
  fixed_pregain_has_changed_ =
      pre_fixed_digital_gain_applier_.GetGainFactor() != gain_factor;
  config_.pre_fixed_digital.gain_factor = gain_factor;
  RTC_DCHECK(Validate(config_))
      << " the invalid config was " << ToString(config_);
  pre_fixed_digital_gain_applier_.SetGainFactor(gain_factor);
  RTC_DCHECK_EQ(pre_fixed_digital_gain_applier_.GetGainFactor(),
                config_.pre_fixed_digital.gain_factor);
  // Reset the limiter to quickly react on abrupt level changes caused by
  // large changes of the fixed gain.
  limiter_.Reset();
}

void GainController2::Initialize(int sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);
  limiter_.SetSampleRate(sample_rate_hz);
  data_dumper_->InitiateNewSetOfRecordings();
  data_dumper_->DumpRaw("sample_rate_hz", sample_rate_hz);
}

bool GainController2::ApplyPreGain(AudioBuffer* audio) {
  if (!config_.enabled) {
    return false;
  }
  const bool pregain_has_changed = fixed_pregain_has_changed_;
  fixed_pregain_has_changed_ = false;
  AudioFrameView<float> float_frame(audio->channels_f(), audio->num_channels(),
                                    audio->num_frames());
  pre_fixed_digital_gain_applier_.ApplyGain(float_frame);
  return pregain_has_changed;
}

void GainController2::ApplyPostGain(AudioBuffer* audio) {
  if (!config_.enabled) {
    return;
  }
  AudioFrameView<float> float_frame(audio->channels_f(), audio->num_channels(),
                                    audio->num_frames());
  // Apply fixed gain first, then the adaptive one.
  fixed_digital_gain_applier_.ApplyGain(float_frame);
  if (config_.adaptive_digital.enabled) {
    adaptive_digital_controller_->Process(float_frame,
                                          limiter_.LastAudioLevel());
  }
  limiter_.Process(float_frame);
}

void GainController2::NotifyAnalogLevel(int level) {
  if (analog_level_ != level && config_.adaptive_digital.enabled) {
    adaptive_digital_controller_->Reset();
  }
  analog_level_ = level;
}

void GainController2::ApplyConfig(
    const AudioProcessing::Config::GainController2& config) {
  RTC_DCHECK(Validate(config))
      << " the invalid config was " << ToString(config);
  config_ = config;
  // Pre-processing.
  pre_fixed_digital_gain_applier_.SetGainFactor(
      config_.pre_fixed_digital.gain_factor);
  // Post-processing.
  fixed_digital_gain_applier_.SetGainFactor(
      DbToRatio(config_.fixed_digital.gain_db));
  adaptive_digital_controller_.reset(
      new AdaptiveAgc(data_dumper_.get(), config_));
  // Reset the limiter to quickly react on abrupt level changes caused by
  // large changes of the fixed gain.
  limiter_.Reset();
}

bool GainController2::Validate(
    const AudioProcessing::Config::GainController2& config) {
  return config.fixed_digital.gain_db >= 0.f &&
         config.fixed_digital.gain_db < 50.f &&
         config.adaptive_digital.extra_saturation_margin_db >= 0.f &&
         config.adaptive_digital.extra_saturation_margin_db <= 100.f;
}

std::string GainController2::ToString(
    const AudioProcessing::Config::GainController2& config) {
  rtc::StringBuilder ss;
  std::string adaptive_digital_level_estimator;
  using LevelEstimatorType =
      AudioProcessing::Config::GainController2::LevelEstimator;
  switch (config.adaptive_digital.level_estimator) {
    case LevelEstimatorType::kRms:
      adaptive_digital_level_estimator = "RMS";
      break;
    case LevelEstimatorType::kPeak:
      adaptive_digital_level_estimator = "peak";
      break;
  }
  // clang-format off
  // clang formatting doesn't respect custom nested style.
  ss << "{"
     << "enabled: " << (config.enabled ? "true" : "false") << ", "
     << "pre_fixed_digital: {"
      << "gain_factor: " << config.pre_fixed_digital.gain_factor << "}, "
     << "fixed_digital: {gain_db: " << config.fixed_digital.gain_db << "}, "
     << "adaptive_digital: {"
      << "enabled: "
        << (config.adaptive_digital.enabled ? "true" : "false") << ", "
      << "level_estimator: " << adaptive_digital_level_estimator << ", "
      << "extra_saturation_margin_db:"
        << config.adaptive_digital.extra_saturation_margin_db << "}"
      << "}";
  // clang-format on
  return ss.Release();
}

}  // namespace webrtc
