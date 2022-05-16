/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_BLOCK_H_
#define MODULES_AUDIO_PROCESSING_AEC3_BLOCK_H_

#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class Block {
 public:
  Block(size_t num_bands, size_t num_channels, float default_value = 0.0f)
      : internal(num_bands,
                 std::vector<std::vector<float>>(
                     num_channels,
                     std::vector<float>(kBlockSize, default_value))) {}

  size_t NumBands() const { return internal.size(); }

  size_t NumChannels() const { return internal[0].size(); }

  void SetNumChannels(size_t num_channels) {
    for (std::vector<std::vector<float>>& block_band : internal) {
      block_band.resize(num_channels, std::vector<float>(kBlockSize, 0.0f));
    }
  }

  auto begin(size_t band, size_t channel) {
    return internal[band][channel].begin();
  }

  auto begin(size_t band, size_t channel) const {
    return internal[band][channel].begin();
  }

  auto end(size_t band, size_t channel) {
    return internal[band][channel].end();
  }

  auto end(size_t band, size_t channel) const {
    return internal[band][channel].end();
  }

  float* FloatArray(size_t band, size_t channel) {
    return internal[band][channel].data();
  }

  const float* FloatArray(size_t band, size_t channel) const {
    return internal[band][channel].data();
  }

  rtc::ArrayView<float> View(size_t band, size_t channel) {
    return rtc::ArrayView<float>(internal[band][channel].data(), kBlockSize);
  }

  rtc::ArrayView<const float> View(size_t band, size_t channel) const {
    return rtc::ArrayView<const float>(internal[band][channel].data(),
                                       kBlockSize);
  }

  // TODO(gustaf): Remove when the transition to Block is completed.
  std::vector<std::vector<std::vector<float>>>* GetInternalData() {
    return &internal;
  }

  // TODO(gustaf): Remove when the transition to Block is completed.
  const std::vector<std::vector<std::vector<float>>>* GetInternalData() const {
    return &internal;
  }

  void Swap(Block& b) { internal.swap(b.internal); }

 private:
  // TODO(gustaf): Improve the internal data structure when the transition to
  // Block is completed.
  std::vector<std::vector<std::vector<float>>> internal;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_PROCESSING_AEC3_BLOCK_H_
