/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/audio_processing_capture_stats.h"

namespace webrtc {

AudioProcessingCaptureStats::AudioProcessingCaptureStats() = default;

AudioProcessingCaptureStats::AudioProcessingCaptureStats(
    const AudioProcessingCaptureStats& other) = default;

AudioProcessingCaptureStats::~AudioProcessingCaptureStats() = default;

}  // namespace webrtc
