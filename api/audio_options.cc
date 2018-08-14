/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_options.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace cricket {
namespace {
template <class T>
std::string ToStringIfSet(absl::string_view key, const absl::optional<T>& val) {
  if (val) {
    return absl::StrCat(key, ": ", *val, ", ");
  }
  return std::string();
}

template <typename T>
void SetFrom(absl::optional<T>* s, const absl::optional<T>& o) {
  if (o) {
    *s = o;
  }
}
}  // namespace

AudioOptions::AudioOptions() = default;
AudioOptions::~AudioOptions() = default;

void AudioOptions::SetAll(const AudioOptions& change) {
  SetFrom(&echo_cancellation, change.echo_cancellation);
#if defined(WEBRTC_IOS)
  SetFrom(&ios_force_software_aec_HACK, change.ios_force_software_aec_HACK);
#endif
  SetFrom(&auto_gain_control, change.auto_gain_control);
  SetFrom(&noise_suppression, change.noise_suppression);
  SetFrom(&highpass_filter, change.highpass_filter);
  SetFrom(&stereo_swapping, change.stereo_swapping);
  SetFrom(&audio_jitter_buffer_max_packets,
          change.audio_jitter_buffer_max_packets);
  SetFrom(&audio_jitter_buffer_fast_accelerate,
          change.audio_jitter_buffer_fast_accelerate);
  SetFrom(&typing_detection, change.typing_detection);
  SetFrom(&aecm_generate_comfort_noise, change.aecm_generate_comfort_noise);
  SetFrom(&experimental_agc, change.experimental_agc);
  SetFrom(&extended_filter_aec, change.extended_filter_aec);
  SetFrom(&delay_agnostic_aec, change.delay_agnostic_aec);
  SetFrom(&experimental_ns, change.experimental_ns);
  SetFrom(&intelligibility_enhancer, change.intelligibility_enhancer);
  SetFrom(&residual_echo_detector, change.residual_echo_detector);
  SetFrom(&tx_agc_target_dbov, change.tx_agc_target_dbov);
  SetFrom(&tx_agc_digital_compression_gain,
          change.tx_agc_digital_compression_gain);
  SetFrom(&tx_agc_limiter, change.tx_agc_limiter);
  SetFrom(&combined_audio_video_bwe, change.combined_audio_video_bwe);
  SetFrom(&audio_network_adaptor, change.audio_network_adaptor);
  SetFrom(&audio_network_adaptor_config, change.audio_network_adaptor_config);
}

bool operator==(const AudioOptions& lhs, const AudioOptions& rhs) {
  return lhs.echo_cancellation == rhs.echo_cancellation &&
#if defined(WEBRTC_IOS)
         lhs.ios_force_software_aec_HACK == rhs.ios_force_software_aec_HACK &&
#endif
         lhs.auto_gain_control == rhs.auto_gain_control &&
         lhs.noise_suppression == rhs.noise_suppression &&
         lhs.highpass_filter == rhs.highpass_filter &&
         lhs.stereo_swapping == rhs.stereo_swapping &&
         lhs.audio_jitter_buffer_max_packets ==
             rhs.audio_jitter_buffer_max_packets &&
         lhs.audio_jitter_buffer_fast_accelerate ==
             rhs.audio_jitter_buffer_fast_accelerate &&
         lhs.typing_detection == rhs.typing_detection &&
         lhs.aecm_generate_comfort_noise == rhs.aecm_generate_comfort_noise &&
         lhs.experimental_agc == rhs.experimental_agc &&
         lhs.extended_filter_aec == rhs.extended_filter_aec &&
         lhs.delay_agnostic_aec == rhs.delay_agnostic_aec &&
         lhs.experimental_ns == rhs.experimental_ns &&
         lhs.intelligibility_enhancer == rhs.intelligibility_enhancer &&
         lhs.residual_echo_detector == rhs.residual_echo_detector &&
         lhs.tx_agc_target_dbov == rhs.tx_agc_target_dbov &&
         lhs.tx_agc_digital_compression_gain ==
             rhs.tx_agc_digital_compression_gain &&
         lhs.tx_agc_limiter == rhs.tx_agc_limiter &&
         lhs.combined_audio_video_bwe == rhs.combined_audio_video_bwe &&
         lhs.audio_network_adaptor == rhs.audio_network_adaptor &&
         lhs.audio_network_adaptor_config == rhs.audio_network_adaptor_config;
}

std::string AudioOptions::ToString() const {
  return absl::StrCat(
      "AudioOptions {", ToStringIfSet("aec", echo_cancellation),
#if defined(WEBRTC_IOS)
      ToStringIfSet("ios_force_software_aec_HACK", ios_force_software_aec_HACK),
#endif
      ToStringIfSet("agc", auto_gain_control),
      ToStringIfSet("ns", noise_suppression),
      ToStringIfSet("hf", highpass_filter),
      ToStringIfSet("swap", stereo_swapping),
      ToStringIfSet("audio_jitter_buffer_max_packets",
                    audio_jitter_buffer_max_packets),
      ToStringIfSet("audio_jitter_buffer_fast_accelerate",
                    audio_jitter_buffer_fast_accelerate),
      ToStringIfSet("typing", typing_detection),
      ToStringIfSet("comfort_noise", aecm_generate_comfort_noise),
      ToStringIfSet("experimental_agc", experimental_agc),
      ToStringIfSet("extended_filter_aec", extended_filter_aec),
      ToStringIfSet("delay_agnostic_aec", delay_agnostic_aec),
      ToStringIfSet("experimental_ns", experimental_ns),
      ToStringIfSet("intelligibility_enhancer", intelligibility_enhancer),
      ToStringIfSet("residual_echo_detector", residual_echo_detector),
      ToStringIfSet("tx_agc_target_dbov", tx_agc_target_dbov),
      ToStringIfSet("tx_agc_digital_compression_gain",
                    tx_agc_digital_compression_gain),
      ToStringIfSet("tx_agc_limiter", tx_agc_limiter),
      ToStringIfSet("combined_audio_video_bwe", combined_audio_video_bwe),
      ToStringIfSet("audio_network_adaptor", audio_network_adaptor), "}");
}

}  // namespace cricket
