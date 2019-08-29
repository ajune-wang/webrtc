/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/bwe_ignore_small_packets_settings.h"

#include "api/transport/field_trial_based_config.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

BweIgnoreSmallPacketsSettings::BweIgnoreSmallPacketsSettings(
    const WebRtcKeyValueConfig* key_value_config)
    : min_fraction_large_packets_("min_fraction_large_packets", 1.0),
      large_packet_size_("large_packet_size", 0),
      ignored_size_("ignored_size", 0) {
  ParseFieldTrial(
      {&min_fraction_large_packets_, &large_packet_size_, &ignored_size_},
      key_value_config->Lookup("WebRTC-BweIgnoreSmallPackets"));
}

BweIgnoreSmallPacketsSettings
BweIgnoreSmallPacketsSettings::ParseFromFieldTrials() {
  FieldTrialBasedConfig field_trial_config;
  return BweIgnoreSmallPacketsSettings(&field_trial_config);
}

}  // namespace webrtc
