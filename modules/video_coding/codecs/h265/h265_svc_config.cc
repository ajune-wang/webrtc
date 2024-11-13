/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/h265/h265_svc_config.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

void SetH265SvcConfig(VideoCodec& video_codec, int num_temporal_layers) {
  RTC_DCHECK_EQ(video_codec.codecType, kVideoCodecH265);

  SpatialLayer& spatial_layer = video_codec.spatialLayers[0];
  spatial_layer.active = true;
  spatial_layer.width = video_codec.width;
  spatial_layer.height = video_codec.height;
  spatial_layer.maxFramerate = video_codec.maxFramerate;
  spatial_layer.numberOfTemporalLayers = num_temporal_layers;
  spatial_layer.minBitrate = video_codec.minBitrate;
  spatial_layer.maxBitrate = video_codec.maxBitrate;
  spatial_layer.targetBitrate =
      (video_codec.minBitrate + video_codec.maxBitrate) / 2;
}

}  // namespace webrtc
