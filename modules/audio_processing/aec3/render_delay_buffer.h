/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_

#include <stddef.h>
#include <array>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/buffer_statistics.h"
#include "modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

// Class for buffering the incoming render blocks such that these may be
// extracted with a specified delay.
class RenderDelayBuffer {
 public:
  enum class BufferingEvent {
    kNone,
    kRenderUnderrun,
    kRenderOverrun,
    kApiCallSkew
  };

  static RenderDelayBuffer* Create(const EchoCanceller3Config& config,
                                   size_t num_bands);
  virtual ~RenderDelayBuffer() = default;

  // Clears the buffer data.
  virtual void Clear() = 0;

  // Resets the buffer alignment.
  virtual void ResetAlignment() = 0;

  // Inserts a block into the buffer and returns true if the insert is
  // successful.
  virtual bool Insert(const std::vector<std::vector<float>>& block) = 0;

  // Updates the buffers one step based on the specified buffer delay. Returns
  // an enum indicating whether there was a special event that occurred.
  virtual BufferingEvent UpdateBuffers() = 0;

  // Sets the buffer delay.
  virtual void SetDelay(size_t delay) = 0;

  // Gets the buffer delay.
  virtual size_t Delay() const = 0;

  // Gets the buffer delay.
  virtual size_t MaxDelay() const = 0;

  // Gets the observed jitter in the render and capture call sequence.
  virtual size_t MaxApiJitter() const = 0;

  // Returns the render buffer for the echo remover.
  virtual const RenderBuffer& GetRenderBuffer() const = 0;

  // Returns the downsampled render buffer.
  virtual const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const = 0;

  // Returns the statistics collector for underruns and overruns.
  virtual const BufferStatistics& GetStatistics() const = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_
