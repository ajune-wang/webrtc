/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/echo_canceller3_config_json.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"

namespace webrtc {
namespace {
void ReadParam(const Json::Value& root, std::string param_name, bool* param) {
  RTC_DCHECK(param);
  bool v;
  if (rtc::GetBoolFromJsonObject(root, param_name, &v)) {
    *param = v;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, size_t* param) {
  RTC_DCHECK(param);
  int v;
  if (rtc::GetIntFromJsonObject(root, param_name, &v) && v >= 0) {
    *param = v;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, int* param) {
  RTC_DCHECK(param);
  int v;
  if (rtc::GetIntFromJsonObject(root, param_name, &v)) {
    *param = v;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, float* param) {
  RTC_DCHECK(param);
  double v;
  if (rtc::GetDoubleFromJsonObject(root, param_name, &v)) {
    *param = static_cast<float>(v);
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Filter::RefinedConfiguration* param) {
  RTC_DCHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 6) {
      RTC_LOG(LS_ERROR) << "Incorrect array size for " << param_name;
      return;
    }
    param->length_blocks = static_cast<size_t>(v[0]);
    param->leakage_converged = static_cast<float>(v[1]);
    param->leakage_diverged = static_cast<float>(v[2]);
    param->error_floor = static_cast<float>(v[3]);
    param->error_ceil = static_cast<float>(v[4]);
    param->noise_gate = static_cast<float>(v[5]);
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Filter::CoarseConfiguration* param) {
  RTC_DCHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 3) {
      RTC_LOG(LS_ERROR) << "Incorrect array size for " << param_name;
      return;
    }
    param->length_blocks = static_cast<size_t>(v[0]);
    param->rate = static_cast<float>(v[1]);
    param->noise_gate = static_cast<float>(v[2]);
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Delay::AlignmentMixing* param) {
  RTC_DCHECK(param);

  Json::Value subsection;
  if (rtc::GetValueFromJsonObject(root, param_name, &subsection)) {
    ReadParam(subsection, "downmix", &param->downmix);
    ReadParam(subsection, "adaptive_selection", &param->adaptive_selection);
    ReadParam(subsection, "activity_power_threshold",
              &param->activity_power_threshold);
    ReadParam(subsection, "prefer_first_two_channels",
              &param->prefer_first_two_channels);
  }
}

void ReadParam(
    const Json::Value& root,
    std::string param_name,
    EchoCanceller3Config::Suppressor::SubbandNearendDetection::SubbandRegion*
        param) {
  RTC_DCHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<int> v;
    rtc::JsonArrayToIntVector(json_array, &v);
    if (v.size() != 2) {
      RTC_LOG(LS_ERROR) << "Incorrect array size for " << param_name;
      return;
    }
    param->low = static_cast<size_t>(v[0]);
    param->high = static_cast<size_t>(v[1]);
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Suppressor::MaskingThresholds* param) {
  RTC_DCHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 3) {
      RTC_LOG(LS_ERROR) << "Incorrect array size for " << param_name;
      return;
    }
    param->enr_transparent = static_cast<float>(v[0]);
    param->enr_suppress = static_cast<float>(v[1]);
    param->emr_transparent = static_cast<float>(v[2]);
  }
}
}  // namespace

void Aec3ConfigFromJsonString(absl::string_view json_string,
                              EchoCanceller3Config* config,
                              bool* parsing_successful) {
  RTC_DCHECK(config);
  RTC_DCHECK(parsing_successful);
  EchoCanceller3Config& cfg = *config;
  cfg = EchoCanceller3Config();
  *parsing_successful = true;

  Json::Value root;
  bool success = Json::Reader().parse(std::string(json_string), root);
  if (!success) {
    RTC_LOG(LS_ERROR) << "Incorrect JSON format: " << json_string;
    *parsing_successful = false;
    return;
  }

  Json::Value aec3_root;
  success = rtc::GetValueFromJsonObject(root, "aec3", &aec3_root);
  if (!success) {
    RTC_LOG(LS_ERROR) << "Missing AEC3 config field: " << json_string;
    *parsing_successful = false;
    return;
  }

  Json::Value section;
  if (rtc::GetValueFromJsonObject(aec3_root, "buffering", &section)) {
    ReadParam(section, "excess_render_detection_interval_blocks",
              &cfg.buffering.excess_render_detection_interval_blocks);
    ReadParam(section, "max_allowed_excess_render_blocks",
              &cfg.buffering.max_allowed_excess_render_blocks);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "delay", &section)) {
    ReadParam(section, "default_delay", &cfg.delay.default_delay);
    ReadParam(section, "down_sampling_factor", &cfg.delay.down_sampling_factor);
    ReadParam(section, "num_filters", &cfg.delay.num_filters);
    ReadParam(section, "delay_headroom_samples",
              &cfg.delay.delay_headroom_samples);
    ReadParam(section, "hysteresis_limit_blocks",
              &cfg.delay.hysteresis_limit_blocks);
    ReadParam(section, "fixed_capture_delay_samples",
              &cfg.delay.fixed_capture_delay_samples);
    ReadParam(section, "delay_estimate_smoothing",
              &cfg.delay.delay_estimate_smoothing);
    ReadParam(section, "delay_candidate_detection_threshold",
              &cfg.delay.delay_candidate_detection_threshold);

    Json::Value subsection;
    if (rtc::GetValueFromJsonObject(section, "delay_selection_thresholds",
                                    &subsection)) {
      ReadParam(subsection, "initial",
                &cfg.delay.delay_selection_thresholds.initial);
      ReadParam(subsection, "converged",
                &cfg.delay.delay_selection_thresholds.converged);
    }

    ReadParam(section, "use_external_delay_estimator",
              &cfg.delay.use_external_delay_estimator);
    ReadParam(section, "log_warning_on_delay_changes",
              &cfg.delay.log_warning_on_delay_changes);

    ReadParam(section, "render_alignment_mixing",
              &cfg.delay.render_alignment_mixing);
    ReadParam(section, "capture_alignment_mixing",
              &cfg.delay.capture_alignment_mixing);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "filter", &section)) {
    ReadParam(section, "refined", &cfg.filter.refined);
    ReadParam(section, "coarse", &cfg.filter.coarse);
    ReadParam(section, "refined_initial", &cfg.filter.refined_initial);
    ReadParam(section, "coarse_initial", &cfg.filter.coarse_initial);
    ReadParam(section, "config_change_duration_blocks",
              &cfg.filter.config_change_duration_blocks);
    ReadParam(section, "initial_state_seconds",
              &cfg.filter.initial_state_seconds);
    ReadParam(section, "conservative_initial_phase",
              &cfg.filter.conservative_initial_phase);
    ReadParam(section, "enable_coarse_filter_output_usage",
              &cfg.filter.enable_coarse_filter_output_usage);
    ReadParam(section, "use_linear_filter", &cfg.filter.use_linear_filter);
    ReadParam(section, "export_linear_aec_output",
              &cfg.filter.export_linear_aec_output);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "erle", &section)) {
    ReadParam(section, "min", &cfg.erle.min);
    ReadParam(section, "max_l", &cfg.erle.max_l);
    ReadParam(section, "max_h", &cfg.erle.max_h);
    ReadParam(section, "onset_detection", &cfg.erle.onset_detection);
    ReadParam(section, "num_sections", &cfg.erle.num_sections);
    ReadParam(section, "clamp_quality_estimate_to_zero",
              &cfg.erle.clamp_quality_estimate_to_zero);
    ReadParam(section, "clamp_quality_estimate_to_one",
              &cfg.erle.clamp_quality_estimate_to_one);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "ep_strength", &section)) {
    ReadParam(section, "default_gain", &cfg.ep_strength.default_gain);
    ReadParam(section, "default_len", &cfg.ep_strength.default_len);
    ReadParam(section, "echo_can_saturate", &cfg.ep_strength.echo_can_saturate);
    ReadParam(section, "bounded_erl", &cfg.ep_strength.bounded_erl);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "echo_audibility", &section)) {
    ReadParam(section, "low_render_limit",
              &cfg.echo_audibility.low_render_limit);
    ReadParam(section, "normal_render_limit",
              &cfg.echo_audibility.normal_render_limit);

    ReadParam(section, "floor_power", &cfg.echo_audibility.floor_power);
    ReadParam(section, "audibility_threshold_lf",
              &cfg.echo_audibility.audibility_threshold_lf);
    ReadParam(section, "audibility_threshold_mf",
              &cfg.echo_audibility.audibility_threshold_mf);
    ReadParam(section, "audibility_threshold_hf",
              &cfg.echo_audibility.audibility_threshold_hf);
    ReadParam(section, "use_stationarity_properties",
              &cfg.echo_audibility.use_stationarity_properties);
    ReadParam(section, "use_stationarity_properties_at_init",
              &cfg.echo_audibility.use_stationarity_properties_at_init);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "render_levels", &section)) {
    ReadParam(section, "active_render_limit",
              &cfg.render_levels.active_render_limit);
    ReadParam(section, "poor_excitation_render_limit",
              &cfg.render_levels.poor_excitation_render_limit);
    ReadParam(section, "poor_excitation_render_limit_ds8",
              &cfg.render_levels.poor_excitation_render_limit_ds8);
    ReadParam(section, "render_power_gain_db",
              &cfg.render_levels.render_power_gain_db);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "echo_removal_control",
                                  &section)) {
    ReadParam(section, "has_clock_drift",
              &cfg.echo_removal_control.has_clock_drift);
    ReadParam(section, "linear_and_stable_echo_path",
              &cfg.echo_removal_control.linear_and_stable_echo_path);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "echo_model", &section)) {
    Json::Value subsection;
    ReadParam(section, "noise_floor_hold", &cfg.echo_model.noise_floor_hold);
    ReadParam(section, "min_noise_floor_power",
              &cfg.echo_model.min_noise_floor_power);
    ReadParam(section, "stationary_gate_slope",
              &cfg.echo_model.stationary_gate_slope);
    ReadParam(section, "noise_gate_power", &cfg.echo_model.noise_gate_power);
    ReadParam(section, "noise_gate_slope", &cfg.echo_model.noise_gate_slope);
    ReadParam(section, "render_pre_window_size",
              &cfg.echo_model.render_pre_window_size);
    ReadParam(section, "render_post_window_size",
              &cfg.echo_model.render_post_window_size);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "comfort_noise", &section)) {
    ReadParam(section, "noise_floor_dbfs", &cfg.comfort_noise.noise_floor_dbfs);
  }

  Json::Value subsection;
  if (rtc::GetValueFromJsonObject(aec3_root, "suppressor", &section)) {
    ReadParam(section, "nearend_average_blocks",
              &cfg.suppressor.nearend_average_blocks);

    if (rtc::GetValueFromJsonObject(section, "normal_tuning", &subsection)) {
      ReadParam(subsection, "mask_lf", &cfg.suppressor.normal_tuning.mask_lf);
      ReadParam(subsection, "mask_hf", &cfg.suppressor.normal_tuning.mask_hf);
      ReadParam(subsection, "max_inc_factor",
                &cfg.suppressor.normal_tuning.max_inc_factor);
      ReadParam(subsection, "max_dec_factor_lf",
                &cfg.suppressor.normal_tuning.max_dec_factor_lf);
    }

    if (rtc::GetValueFromJsonObject(section, "nearend_tuning", &subsection)) {
      ReadParam(subsection, "mask_lf", &cfg.suppressor.nearend_tuning.mask_lf);
      ReadParam(subsection, "mask_hf", &cfg.suppressor.nearend_tuning.mask_hf);
      ReadParam(subsection, "max_inc_factor",
                &cfg.suppressor.nearend_tuning.max_inc_factor);
      ReadParam(subsection, "max_dec_factor_lf",
                &cfg.suppressor.nearend_tuning.max_dec_factor_lf);
    }

    if (rtc::GetValueFromJsonObject(section, "dominant_nearend_detection",
                                    &subsection)) {
      ReadParam(subsection, "enr_threshold",
                &cfg.suppressor.dominant_nearend_detection.enr_threshold);
      ReadParam(subsection, "enr_exit_threshold",
                &cfg.suppressor.dominant_nearend_detection.enr_exit_threshold);
      ReadParam(subsection, "snr_threshold",
                &cfg.suppressor.dominant_nearend_detection.snr_threshold);
      ReadParam(subsection, "hold_duration",
                &cfg.suppressor.dominant_nearend_detection.hold_duration);
      ReadParam(subsection, "trigger_threshold",
                &cfg.suppressor.dominant_nearend_detection.trigger_threshold);
      ReadParam(
          subsection, "use_during_initial_phase",
          &cfg.suppressor.dominant_nearend_detection.use_during_initial_phase);
    }

    if (rtc::GetValueFromJsonObject(section, "subband_nearend_detection",
                                    &subsection)) {
      ReadParam(
          subsection, "nearend_average_blocks",
          &cfg.suppressor.subband_nearend_detection.nearend_average_blocks);
      ReadParam(subsection, "subband1",
                &cfg.suppressor.subband_nearend_detection.subband1);
      ReadParam(subsection, "subband2",
                &cfg.suppressor.subband_nearend_detection.subband2);
      ReadParam(subsection, "nearend_threshold",
                &cfg.suppressor.subband_nearend_detection.nearend_threshold);
      ReadParam(subsection, "snr_threshold",
                &cfg.suppressor.subband_nearend_detection.snr_threshold);
    }

    ReadParam(section, "use_subband_nearend_detection",
              &cfg.suppressor.use_subband_nearend_detection);

    if (rtc::GetValueFromJsonObject(section, "high_bands_suppression",
                                    &subsection)) {
      ReadParam(subsection, "enr_threshold",
                &cfg.suppressor.high_bands_suppression.enr_threshold);
      ReadParam(subsection, "max_gain_during_echo",
                &cfg.suppressor.high_bands_suppression.max_gain_during_echo);
      ReadParam(subsection, "anti_howling_activation_threshold",
                &cfg.suppressor.high_bands_suppression
                     .anti_howling_activation_threshold);
      ReadParam(subsection, "anti_howling_gain",
                &cfg.suppressor.high_bands_suppression.anti_howling_gain);
    }

    ReadParam(section, "floor_first_increase",
              &cfg.suppressor.floor_first_increase);
  }
}

EchoCanceller3Config Aec3ConfigFromJsonString(absl::string_view json_string) {
  EchoCanceller3Config cfg;
  bool not_used;
  Aec3ConfigFromJsonString(json_string, &cfg, &not_used);
  return cfg;
}

std::string Aec3ConfigToJsonString(const EchoCanceller3Config& config) {
  auto fmt_bool = [](bool x) { return x ? "true" : "false"; };
  return absl::StrFormat(
      R"("aec3": {"buffering":)"
      R"({"excess_render_detection_interval_blocks": %zu,)"
      R"("max_allowed_excess_render_blocks": %zu},)"
      R"("delay": {"default_delay": %zu,)"
      R"("down_sampling_factor": %zu,)"
      R"("num_filters": %zu,)"
      R"("delay_headroom_samples": %zu,)"
      R"("hysteresis_limit_blocks": %zu,)"
      R"("fixed_capture_delay_samples": %zu,)"
      R"("delay_estimate_smoothing": %f,)"
      R"("delay_candidate_detection_threshold": %f,)"
      R"("delay_selection_thresholds": {"initial": %d,)"
      R"("converged": %d},)"
      R"("use_external_delay_estimator": %s,)"
      R"("log_warning_on_delay_changes": %s,)"
      R"("render_alignment_mixing": {"downmix": %s,)"
      R"("adaptive_selection": %s,)"
      R"("activity_power_threshold": %f,)"
      R"("prefer_first_two_channels": %s},)"
      R"("capture_alignment_mixing": {"downmix": %s,)"
      R"("adaptive_selection": %s,)"
      R"("activity_power_threshold": %f,)"
      R"("prefer_first_two_channels": %s}},)"
      R"("filter": {refined": [%zu,%f,%f,%f,%f,%f],)"
      R"("coarse": [%zu,%f,%f],)"
      R"("refined_initial": [%zu,%f,%f,%f,%f,%f],)"
      R"("coarse_initial": [%zu,%f,%f],)"
      R"("config_change_duration_blocks": %zu,)"
      R"("initial_state_seconds": %f,)"
      R"("conservative_initial_phase": %s,)"
      R"("enable_coarse_filter_output_usage": %s,)"
      R"("use_linear_filter": %s,)"
      R"("export_linear_aec_output": %s,)"
      R"("erle": {"min": %f,)"
      R"("max_l": %f,)"
      R"("max_h": %f,)"
      R"("onset_detection": %s,)"
      R"("num_sections": %zu,)"
      R"("clamp_quality_estimate_to_zero": %s,)"
      R"("clamp_quality_estimate_to_one": %s},)"
      R"("ep_strength": {"default_gain": %f,)"
      R"("default_len": %f,)"
      R"("echo_can_saturate": %s,)"
      R"("bounded_erl": %s},)"
      R"("echo_audibility": {"low_render_limit": %f,)"
      R"("normal_render_limit": %f,)"
      R"("floor_power": %f,)"
      R"("audibility_threshold_lf": %f,)"
      R"("audibility_threshold_mf": %f,)"
      R"("audibility_threshold_hf": %f,)"
      R"("use_stationarity_properties": %s,)"
      R"("use_stationarity_properties_at_init": %s},)"
      R"("render_levels": {"active_render_limit": %f,)"
      R"("poor_excitation_render_limit": %f,)"
      R"("poor_excitation_render_limit_ds8": %f,)"
      R"("render_power_gain_db": %f},)"
      R"("echo_removal_control": {"has_clock_drift": %s,)"
      R"("linear_and_stable_echo_path": %s},)"
      R"("echo_model": {"noise_floor_hold": %zu,)"
      R"("min_noise_floor_power": %f,)"
      R"("stationary_gate_slope": %f,)"
      R"("noise_gate_power": %f,)"
      R"("noise_gate_slope": %f,)"
      R"("render_pre_window_size": %zu,)"
      R"("render_post_window_size": %zu},)"
      R"("comfort_noise": {"noise_floor_dbfs": %f},)"
      R"("suppressor": {"nearend_average_blocks": %zu,)"
      R"("normal_tuning": {"mask_lf": [%f,%f,%f],)"
      R"("mask_hf": [%f,%f,%f],)"
      R"("max_inc_factor": %f,)"
      R"("max_dec_factor_lf": %f},)"
      R"("nearend_tuning": {"mask_lf": [%f,%f,%f],)"
      R"("mask_hf": [%f,%f,%f],)"
      R"("max_inc_factor": %f,)"
      R"("max_dec_factor_lf": %f},)"
      R"("dominant_nearend_detection": {"enr_threshold": %f,)"
      R"("enr_exit_threshold": %f,)"
      R"("snr_threshold": %f,)"
      R"("hold_duration": %d,)"
      R"("trigger_threshold": %d,)"
      R"("use_during_initial_phase": %d},)"
      R"("subband_nearend_detection": {"nearend_average_blocks": %zu,)"
      R"("subband1": [%zu,%zu],)"
      R"("subband2": [%zu,%zu],)"
      R"("nearend_threshold": %f,)"
      R"("snr_threshold": %f},)"
      R"("use_subband_nearend_detection": %d,)"
      R"("high_bands_suppression": {"enr_threshold": %f,)"
      R"("max_gain_during_echo": %f,)"
      R"("anti_howling_activation_threshold": %f,)"
      R"("anti_howling_gain": %f},)"
      R"("floor_first_increase": %f}}})",
      config.buffering.excess_render_detection_interval_blocks,
      config.buffering.max_allowed_excess_render_blocks,
      config.delay.default_delay, config.delay.down_sampling_factor,
      config.delay.num_filters, config.delay.delay_headroom_samples,
      config.delay.hysteresis_limit_blocks,
      config.delay.fixed_capture_delay_samples,
      config.delay.delay_estimate_smoothing,
      config.delay.delay_candidate_detection_threshold,
      config.delay.delay_selection_thresholds.initial,
      config.delay.delay_selection_thresholds.converged,
      fmt_bool(config.delay.use_external_delay_estimator),
      fmt_bool(config.delay.log_warning_on_delay_changes),
      fmt_bool(config.delay.render_alignment_mixing.downmix),
      fmt_bool(config.delay.render_alignment_mixing.adaptive_selection),
      config.delay.render_alignment_mixing.activity_power_threshold,
      fmt_bool(config.delay.render_alignment_mixing.prefer_first_two_channels),
      fmt_bool(config.delay.capture_alignment_mixing.downmix),
      fmt_bool(config.delay.capture_alignment_mixing.adaptive_selection),
      config.delay.capture_alignment_mixing.activity_power_threshold,
      fmt_bool(config.delay.capture_alignment_mixing.prefer_first_two_channels),
      config.filter.refined.length_blocks,
      config.filter.refined.leakage_converged,
      config.filter.refined.leakage_diverged, config.filter.refined.error_floor,
      config.filter.refined.error_ceil, config.filter.refined.noise_gate,
      config.filter.coarse.length_blocks, config.filter.coarse.rate,
      config.filter.coarse.noise_gate,
      config.filter.refined_initial.length_blocks,
      config.filter.refined_initial.leakage_converged,
      config.filter.refined_initial.leakage_diverged,
      config.filter.refined_initial.error_floor,
      config.filter.refined_initial.error_ceil,
      config.filter.refined_initial.noise_gate,
      config.filter.coarse_initial.length_blocks,
      config.filter.coarse_initial.rate,
      config.filter.coarse_initial.noise_gate,
      config.filter.config_change_duration_blocks,
      config.filter.initial_state_seconds,
      fmt_bool(config.filter.conservative_initial_phase),
      fmt_bool(config.filter.enable_coarse_filter_output_usage),
      fmt_bool(config.filter.use_linear_filter),
      fmt_bool(config.filter.export_linear_aec_output), config.erle.min,
      config.erle.max_l, config.erle.max_h,
      fmt_bool(config.erle.onset_detection), config.erle.num_sections,
      fmt_bool(config.erle.clamp_quality_estimate_to_zero),
      fmt_bool(config.erle.clamp_quality_estimate_to_one),
      config.ep_strength.default_gain, config.ep_strength.default_len,
      fmt_bool(config.ep_strength.echo_can_saturate),
      fmt_bool(config.ep_strength.bounded_erl),
      config.echo_audibility.low_render_limit,
      config.echo_audibility.normal_render_limit,
      config.echo_audibility.floor_power,
      config.echo_audibility.audibility_threshold_lf,
      config.echo_audibility.audibility_threshold_mf,
      config.echo_audibility.audibility_threshold_hf,
      fmt_bool(config.echo_audibility.use_stationarity_properties),
      fmt_bool(config.echo_audibility.use_stationarity_properties_at_init),
      config.render_levels.active_render_limit,
      config.render_levels.poor_excitation_render_limit,
      config.render_levels.poor_excitation_render_limit_ds8,
      config.render_levels.render_power_gain_db,
      fmt_bool(config.echo_removal_control.has_clock_drift),
      fmt_bool(config.echo_removal_control.linear_and_stable_echo_path),
      config.echo_model.noise_floor_hold,
      config.echo_model.min_noise_floor_power,
      config.echo_model.stationary_gate_slope,
      config.echo_model.noise_gate_power, config.echo_model.noise_gate_slope,
      config.echo_model.render_pre_window_size,
      config.echo_model.render_post_window_size,
      config.comfort_noise.noise_floor_dbfs,
      config.suppressor.nearend_average_blocks,
      config.suppressor.normal_tuning.mask_lf.enr_transparent,
      config.suppressor.normal_tuning.mask_lf.enr_suppress,
      config.suppressor.normal_tuning.mask_lf.emr_transparent,
      config.suppressor.normal_tuning.mask_hf.enr_transparent,
      config.suppressor.normal_tuning.mask_hf.enr_suppress,
      config.suppressor.normal_tuning.mask_hf.emr_transparent,
      config.suppressor.normal_tuning.max_inc_factor,
      config.suppressor.normal_tuning.max_dec_factor_lf,
      config.suppressor.nearend_tuning.mask_lf.enr_transparent,
      config.suppressor.nearend_tuning.mask_lf.enr_suppress,
      config.suppressor.nearend_tuning.mask_lf.emr_transparent,
      config.suppressor.nearend_tuning.mask_hf.enr_transparent,
      config.suppressor.nearend_tuning.mask_hf.enr_suppress,
      config.suppressor.nearend_tuning.mask_hf.emr_transparent,
      config.suppressor.nearend_tuning.max_inc_factor,
      config.suppressor.nearend_tuning.max_dec_factor_lf,
      config.suppressor.dominant_nearend_detection.enr_threshold,
      config.suppressor.dominant_nearend_detection.enr_exit_threshold,
      config.suppressor.dominant_nearend_detection.snr_threshold,
      config.suppressor.dominant_nearend_detection.hold_duration,
      config.suppressor.dominant_nearend_detection.trigger_threshold,
      config.suppressor.dominant_nearend_detection.use_during_initial_phase,
      config.suppressor.subband_nearend_detection.nearend_average_blocks,
      config.suppressor.subband_nearend_detection.subband1.low,
      config.suppressor.subband_nearend_detection.subband1.high,
      config.suppressor.subband_nearend_detection.subband2.low,
      config.suppressor.subband_nearend_detection.subband2.high,
      config.suppressor.subband_nearend_detection.nearend_threshold,
      config.suppressor.subband_nearend_detection.snr_threshold,
      config.suppressor.use_subband_nearend_detection,
      config.suppressor.high_bands_suppression.enr_threshold,
      config.suppressor.high_bands_suppression.max_gain_during_echo,
      config.suppressor.high_bands_suppression
          .anti_howling_activation_threshold,
      config.suppressor.high_bands_suppression.anti_howling_gain,
      config.suppressor.floor_first_increase);
}
}  // namespace webrtc
