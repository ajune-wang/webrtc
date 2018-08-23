/* Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
/*
 * This file defines the interface for doing temporal layers with VP8.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_

#include <vector>
#include <memory>

#include "api/video_codecs/video_codec.h"

#define VP8_TS_MAX_PERIODICITY 16
#define VP8_TS_MAX_LAYERS 5

namespace webrtc {

// Some notes on the prerequisites of the TemporalLayers interface.
// * Implementations of TemporalLayers may not contain internal synchronization
//   so caller must make sure doing so thread safe.
// * The encoder is assumed to encode all frames in order, and callbacks to
//   PopulateCodecSpecific() / FrameEncoded() must happen in the same order.
//
// This means that in the case of pipelining encoders, it is OK to have a chain
// of calls such as this:
// - UpdateLayerConfig(timestampA)
// - UpdateLayerConfig(timestampB)
// - OnEncodeDone(timestampA, 1234, ...)
// - UpdateLayerConfig(timestampC)
// - OnEncodeDone(timestampB, 0, ...)
// - OnEncodeDone(timestampC, 1234, ...)
// Note that UpdateLayerConfig() for a new frame can happen before
// FrameEncoded() for a previous one, but calls themselves must be both
// synchronized (e.g. run on a task queue) and in order (per type).

struct CodecSpecificInfoVP8;
enum class Vp8BufferReference : uint8_t {
  kNone = 0,
  kLast = 1,
  kGolden = 2,
  kAltref = 4
};

struct Vp8EncoderConfig {
  // Number of active temporal layers. Set to 0 if not used.
  unsigned int ts_number_layers;
  // Arrays of length |ts_number_layers|, indicating (cumulative) target bitrate
  // and rate decimator (e.g. 4 if every 4th frame is in the given layer) for
  // each active temporal layer, starting with temporal id 0.
  unsigned int ts_target_bitrate[VP8_TS_MAX_LAYERS];
  unsigned int ts_rate_decimator[VP8_TS_MAX_LAYERS];

  // The periodicity of the temporal pattern. Set to 0 if not used.
  unsigned int ts_periodicity;
  // Array of length |ts_periodicity| indicating the sequence of temporal id's
  // to assign to incoming frames.
  unsigned int ts_layer_id[VP8_TS_MAX_PERIODICITY];

  // Target bitrate, in bps.
  unsigned int rc_target_bitrate;

  // Clamp QP to min/max. Use 0 to disable clamping.
  unsigned int rc_min_quantizer;
  unsigned int rc_max_quantizer;
};

// This interface defines a way of getting the encoder settings needed to
// realize a temporal layer structure of predefined size.
class TemporalLayersChecker;
class TemporalLayers {
 public:
  enum BufferFlags : int {
    kNone = 0,
    kReference = 1,
    kUpdate = 2,
    kReferenceAndUpdate = kReference | kUpdate,
  };
  enum FreezeEntropy { kFreezeEntropy };

  struct FrameConfig {
    FrameConfig();

    FrameConfig(BufferFlags last, BufferFlags golden, BufferFlags arf);
    FrameConfig(BufferFlags last,
                BufferFlags golden,
                BufferFlags arf,
                FreezeEntropy);

    bool drop_frame;
    BufferFlags last_buffer_flags;
    BufferFlags golden_buffer_flags;
    BufferFlags arf_buffer_flags;

    // The encoder layer ID is used to utilize the correct bitrate allocator
    // inside the encoder. It does not control references nor determine which
    // "actual" temporal layer this is. The packetizer temporal index determines
    // which layer the encoded frame should be packetized into.
    // Normally these are the same, but current temporal-layer strategies for
    // screenshare use one bitrate allocator for all layers, but attempt to
    // packetize / utilize references to split a stream into multiple layers,
    // with different quantizer settings, to hit target bitrate.
    // TODO(pbos): Screenshare layers are being reconsidered at the time of
    // writing, we might be able to remove this distinction, and have a temporal
    // layer imply both (the normal case).
    int encoder_layer_id;
    int packetizer_temporal_idx;

    bool layer_sync;

    bool freeze_entropy;

    // Indicates in which order the encoder should search the reference buffers
    // when doing motion prediction. Set to kNone to use unspecified order. Any
    // buffer indicated here must not have the corresponding no_ref bit set.
    // If all three buffers can be reference, the one not listed here should be
    // searched last.
    Vp8BufferReference first_reference;
    Vp8BufferReference second_reference;

    bool operator==(const FrameConfig& o) const;
    bool operator!=(const FrameConfig& o) const { return !(*this == o); }

   private:
    FrameConfig(BufferFlags last,
                BufferFlags golden,
                BufferFlags arf,
                bool freeze_entropy);
  };

  // Factory for TemporalLayer strategy. Default behavior is a fixed pattern
  // of temporal layers. See default_temporal_layers.cc
  static std::unique_ptr<TemporalLayers> CreateTemporalLayers(
      const VideoCodec& codec,
      size_t spatial_id);
  static std::unique_ptr<TemporalLayersChecker> CreateTemporalLayersChecker(
      const VideoCodec& codec,
      size_t spatial_id);

  virtual ~TemporalLayers() = default;

  // If this method returns true, the encoder is free to drop frames for
  // instance in an effort to uphold encoding bitrate.
  // If this return false, the encoder must not drop any frames unless:
  //  1. Requested to do so via FrameConfig.drop_frame
  //  2. The frame to be encoded is requested to be a keyframe
  //  3. The encoded detected a large overshoot and decided to drop and then
  //     re-encode the image at a low bitrate. In this case the encoder should
  //     call OnEncodeDone() once with size = 0 to indicate drop, and then call
  //     OnEncodeDone() again when the frame has actually been encoded.
  virtual bool SupportsEncoderFrameDropping() const = 0;

  // New target bitrate, per temporal layer.
  virtual void OnRatesUpdated(const std::vector<uint32_t>& bitrates_bps,
                              int framerate_fps) = 0;

  // Called by the encoder before encoding a frame. |cfg| contains the current
  // configuration. If the TemporalLayers instance wishes any part of that
  // to be changed before the encode step, |cfg| should be changed and then
  // return true. If false is returned, the encoder will proceed without
  // updating the configuration.
  virtual bool UpdateConfiguration(Vp8EncoderConfig* cfg) = 0;

  // Returns the recommended VP8 encode flags needed, and moves the temporal
  // pattern to the next frame.
  // The timestamp may be used as both a time and a unique identifier, and so
  // the caller must make sure no two frames use the same timestamp.
  // The timestamp uses a 90kHz RTP clock.
  // After calling this method, the actual encoder should be called with the
  // provided frame configuration, after which:
  // * On success, call PopulateCodecSpecific() and then FrameEncoded();
  // * On failure/ frame drop: Call FrameEncoded() with size = 0.
  virtual FrameConfig UpdateLayerConfig(uint32_t rtp_timestamp) = 0;

  // Called after the encode step is done. |rtp_timestamp| must match the
  // parameter use in the UpdateLayerConfig() call.
  // |is_keyframe| must be true iff the encoder decided to encode this frame as
  // a keyframe.
  // If the encoder decided to drop this frame, |size_bytes| must be set to 0,
  // otherwise it should indicate the size in bytes of the encoded frame.
  // If |size_bytes| > 0, and |vp8_info| is not null, the TemporalLayers
  // instance my update |vp8_info| with codec specific data such as temporal id.
  // Some fields of this struct may have already been populated by the encoder,
  // check before overwriting.
  // If |size_bytes| > 0, |qp| should indicate the frame-level QP this frame was
  // encoded at. If the encoder does not support extracting this, |qp| should be
  // set to 0.
  virtual void OnEncodeDone(uint32_t rtp_timestamp,
                            size_t size_bytes,
                            bool is_keyframe,
                            int qp,
                            CodecSpecificInfoVP8* vp8_info) = 0;
};

// Used only inside RTC_DCHECK(). It checks correctness of temporal layers
// dependencies and sync bits. The only method of this class is called after
// each UpdateLayersConfig() of a corresponding TemporalLayers class.
class TemporalLayersChecker {
 public:
  explicit TemporalLayersChecker(int num_temporal_layers);
  virtual ~TemporalLayersChecker() {}

  virtual bool CheckTemporalConfig(
      bool frame_is_keyframe,
      const TemporalLayers::FrameConfig& frame_config);

 private:
  struct BufferState {
    BufferState() : is_keyframe(true), temporal_layer(0), sequence_number(0) {}
    bool is_keyframe;
    uint8_t temporal_layer;
    uint32_t sequence_number;
  };
  bool CheckAndUpdateBufferState(BufferState* state,
                                 bool* need_sync,
                                 bool frame_is_keyframe,
                                 uint8_t temporal_layer,
                                 webrtc::TemporalLayers::BufferFlags flags,
                                 uint32_t sequence_number,
                                 uint32_t* lowest_sequence_referenced);
  BufferState last_;
  BufferState arf_;
  BufferState golden_;
  int num_temporal_layers_;
  uint32_t sequence_number_;
  uint32_t last_sync_sequence_number_;
  uint32_t last_tl0_sequence_number_;
};

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_
