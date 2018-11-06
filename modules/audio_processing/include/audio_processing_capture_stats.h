/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_CAPTURE_STATS_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_CAPTURE_STATS_H_

#include "absl/types/optional.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {
struct RTC_EXPORT AudioProcessingCaptureStats {
  AudioProcessingCaptureStats();
  AudioProcessingCaptureStats(const AudioProcessingCaptureStats& other);
  ~AudioProcessingCaptureStats();

  // The root mean square (RMS) level in dBFs (decibels from digital
  // full-scale) of the last capture frame, after processing. It is
  // constrained to [-127, 0].
  // The computation follows: https://tools.ietf.org/html/rfc6465
  // with the intent that it can provide the RTP audio level indication.
  //  Updated if level estimation is enabled.
  // TODO(bugs.webrtc.org/9947): Not yet in use.
  absl::optional<int> output_rms_dbfs;
  // The speech probability of the last processed capture frame, as used
  // inside the noise suppressor. Averaged over all channels.
  // Updated if noise suppression is enabled.
  // TODO(bugs.webrtc.org/9947): Not yet in use.
  absl::optional<float> speech_probability;
  // Whether voice was detected in the last processed capture frame.
  // Updated if voice detection is enabled.
  // TODO(bugs.webrtc.org/9947): Not yet in use.
  absl::optional<bool> has_voice;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_CAPTURE_STATS_H_
