/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VP8_TEMPORAL_LAYERS_H_
#define API_VIDEO_CODECS_VP8_TEMPORAL_LAYERS_H_

#include <vector>

#include "api/video_codecs/vp8_frame_buffer_controller.h"
#include "api/video_codecs/vp8_frame_config.h"

namespace webrtc {

// Two different flavors of temporal layers are currently available:
// kFixedPattern uses a fixed repeating pattern of 1-4 layers.
// kBitrateDynamic can allocate frames dynamically to 1 or 2 layers, based on
// the bitrate produced.
enum class Vp8TemporalLayersType { kFixedPattern, kBitrateDynamic };

// This interface defines a way of getting the encoder settings needed to
// realize a temporal layer structure.
class Vp8TemporalLayers : public Vp8FrameBufferController {
 public:
  ~Vp8TemporalLayers() override = default;

  // If this method returns true, the encoder is free to drop frames for
  // instance in an effort to uphold encoding bitrate.
  // If this return false, the encoder must not drop any frames unless:
  //  1. Requested to do so via Vp8FrameConfig.drop_frame
  //  2. The frame to be encoded is requested to be a keyframe
  //  3. The encoded detected a large overshoot and decided to drop and then
  //     re-encode the image at a low bitrate. In this case the encoder should
  //     call OnEncodeDone() once with size = 0 to indicate drop, and then call
  //     OnEncodeDone() again when the frame has actually been encoded.
  bool SupportsEncoderFrameDropping() const override = 0;

  // New target bitrate, per temporal layer.
  void OnRatesUpdated(const std::vector<uint32_t>& bitrates_bps,
                      int framerate_fps) override = 0;

  // Called by the encoder before encoding a frame. |cfg| contains the current
  // configuration. If the TemporalLayers instance wishes any part of that
  // to be changed before the encode step, |cfg| should be changed and then
  // return true. If false is returned, the encoder will proceed without
  // updating the configuration.
  bool UpdateConfiguration(Vp8EncoderConfig* cfg) override = 0;

  // Returns the recommended VP8 encode flags needed, and moves the temporal
  // pattern to the next frame.
  // The timestamp may be used as both a time and a unique identifier, and so
  // the caller must make sure no two frames use the same timestamp.
  // The timestamp uses a 90kHz RTP clock.
  // After calling this method, first call the actual encoder with the provided
  // frame configuration, and then OnEncodeDone() below.
  Vp8FrameConfig UpdateLayerConfig(uint32_t rtp_timestamp) override = 0;

  // Called after the encode step is done. |rtp_timestamp| must match the
  // parameter use in the UpdateLayerConfig() call.
  // |is_keyframe| must be true iff the encoder decided to encode this frame as
  // a keyframe.
  // If the encoder decided to drop this frame, |size_bytes| must be set to 0,
  // otherwise it should indicate the size in bytes of the encoded frame.
  // If |size_bytes| > 0, and |info| is not null, the TemporalLayers
  // instance my update |info| with codec specific data such as temporal id.
  // Some fields of this struct may have already been populated by the encoder,
  // check before overwriting.
  // If |size_bytes| > 0, |qp| should indicate the frame-level QP this frame was
  // encoded at. If the encoder does not support extracting this, |qp| should be
  // set to 0.
  void OnEncodeDone(uint32_t rtp_timestamp,
                    size_t size_bytes,
                    bool is_keyframe,
                    int qp,
                    CodecSpecificInfo* info) override = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VP8_TEMPORAL_LAYERS_H_
