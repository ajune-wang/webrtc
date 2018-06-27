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

#include "rtc_base/absl_str_cat.h"

namespace cricket {

AudioOptions::AudioOptions() = default;
AudioOptions::~AudioOptions() = default;

std::string AudioOptions::ToString() const {
  return absl::StrCat(
      "AudioOptions {", ToStringIfSet("aec", echo_cancellation)
#if defined(WEBRTC_IOS)
                            ,
      ToStringIfSet("ios_force_software_aec_HACK", ios_force_software_aec_HACK)
#endif
          ,
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
