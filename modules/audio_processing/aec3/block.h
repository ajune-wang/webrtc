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

#include <array>
#include <vector>

#include "api/audio/audio_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/checks.h"

namespace webrtc {

// Contains one or more channels of 4 milliseconds of audio data.
// The audio is split in one or more frequency bands, each with a sampling
// rate of 16 kHz.
class Block {
 public:
  Block(size_t num_bands, size_t num_channels, float default_value = 0.0f)
      : num_bands_(num_bands),
        num_channels_(num_channels),
        data_(num_bands * num_channels * kBlockSize, default_value) {
    RTC_DCHECK_NE(num_channels, 0u);
    RTC_DCHECK_NE(num_bands, 0u);
  }

  Block(const Block& block) = default;
  Block(Block&& block) = default;
  Block& operator=(Block&& block) = default;

  // Returns the number of bands.
  size_t NumBands() const { return num_bands_; }

  // Returns the number of channels.
  size_t NumChannels() const { return num_channels_; }

  // Iterators for accessing the data.
  auto begin(int band, int channel) {
    return data_.begin() + GetIndex(band, channel);
  }

  auto begin(int band, int channel) const {
    return data_.begin() + GetIndex(band, channel);
  }

  auto end(int band, int channel) { return begin(band, channel) + kBlockSize; }

  auto end(int band, int channel) const {
    return begin(band, channel) + kBlockSize;
  }

  // Access data via ArrayView.
  MonoView<float, kBlockSize> View(size_t band, size_t channel) {
    return MonoView<float, kBlockSize>(&data_[GetIndex(band, channel)],
                                       kBlockSize);
  }

  MonoView<const float, kBlockSize> View(size_t band, size_t channel) const {
    return MonoView<const float, kBlockSize>(&data_[GetIndex(band, channel)],
                                             kBlockSize);
  }

 private:
  // Returns the index of the first sample of the requested |band| and
  // |channel|.
  int GetIndex(int band, int channel) const {
    return (band * num_channels_ + channel) * kBlockSize;
  }

  // All members are set at construction and should for the most part be
  // considered `const`. The reason for why they're not const is to allow for
  // moving state out of the class, to [re]construct another instance.
  size_t num_bands_;
  size_t num_channels_;
  std::vector<float> data_;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_PROCESSING_AEC3_BLOCK_H_
