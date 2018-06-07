/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_
#define API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_

#include <map>
#include <memory>
#include <utility>

#include "api/video/video_stream_encoder_interface.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

struct VideoStreamEncoderSettings {
  VideoStreamEncoderSettings() = default;

  // Enables the new method to estimate the cpu load from encoding, used for
  // cpu adaptation.
  bool experiment_cpu_load_estimator = false;

  // Enables hardware VAAPI VP8 encoding if supported by the provided
  // VideoEncoderFactory.
  // TODO(ilnik): remove this when VAAPI VP8 experiment is over.
  bool experiment_vaapi_vp8_hw_encoding = false;

  // Ownership stays with WebrtcVideoEngine (delegated from PeerConnection).
  VideoEncoderFactory* encoder_factory = nullptr;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_
