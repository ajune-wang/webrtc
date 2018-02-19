/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_BITRATE_CONFIGURATOR_H_
#define CALL_RTP_BITRATE_CONFIGURATOR_H_

#include "call/bitrate_config.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
class RtpBitrateConfigurator {
 public:
  explicit RtpBitrateConfigurator(const BitrateConfig& bitrate_config);
  ~RtpBitrateConfigurator();
  BitrateConfig GetConfig() const;

  rtc::Optional<BitrateConfig> UpdateBitrateConfig(
      const BitrateConfig& bitrate_config_);
  rtc::Optional<BitrateConfig> UpdateBitrateConfigMask(
      const BitrateConfigMask& bitrate_mask);

 private:
  rtc::Optional<BitrateConfig> UpdateCurrentBitrateConfig(
      const rtc::Optional<int>& new_start);

  // Bitrate config used until valid bitrate estimates are calculated. Also
  // used to cap total bitrate used. This comes from the remote connection.
  BitrateConfig bitrate_config_;

  // The config mask set by SetBitrateConfigMask.
  // 0 <= min <= start <= max
  BitrateConfigMask bitrate_config_mask_;

  // The config set by SetBitrateConfig.
  // min >= 0, start != 0, max == -1 || max > 0
  BitrateConfig base_bitrate_config_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpBitrateConfigurator);
};
}  // namespace webrtc

#endif  // CALL_RTP_BITRATE_CONFIGURATOR_H_
