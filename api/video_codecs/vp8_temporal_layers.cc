/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/vp8_temporal_layers.h"

namespace webrtc {

void Vp8TemporalLayers::OnPacketLossRateUpdate(float packet_loss_rate) {}

void Vp8TemporalLayers::OnRttUpdate(int64_t rtt_ms) {}

}  // namespace webrtc
