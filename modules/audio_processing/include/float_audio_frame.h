/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_FLOAT_AUDIO_FRAME_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_FLOAT_AUDIO_FRAME_H_

#include "api/array_view.h"

class MutableFloatAudioFrame {
  friend class FloatAudioFrame;

 public:
  // |num_channels| and |channel_size| describe the float**
  // |audio_samples|. |audio_samples| is assumed to point to a
  // two-dimensional |num_channels * channel_size| array of floats.
  MutableFloatAudioFrame(float* const* audio_samples,
                         size_t num_channels,
                         size_t channel_size)
      : audio_samples_(audio_samples),
        num_channels_(num_channels),
        channel_size_(channel_size) {}

  MutableFloatAudioFrame() = delete;

  size_t num_channels() const { return num_channels_; }

  size_t samples_per_channel() const { return channel_size_; }

  rtc::ArrayView<float> channel(size_t idx) {
    RTC_DCHECK_LE(0, idx);
    RTC_DCHECK_LE(idx, num_channels_);
    return rtc::ArrayView<float>(audio_samples_[idx], channel_size_);
  }

  rtc::ArrayView<const float> channel(size_t idx) const {
    RTC_DCHECK_LE(0, idx);
    RTC_DCHECK_LE(idx, num_channels_);
    return rtc::ArrayView<const float>(audio_samples_[idx], channel_size_);
  }

 private:
  float* const* audio_samples_;
  size_t num_channels_;
  size_t channel_size_;
};

// Class to pass audio data in float** format. This is to avoid
// dependence on AudioBuffer, and avoid problems associated with
// rtc::ArrayView<rtc::ArrayView>.
class FloatAudioFrame {
 public:
  // |num_channels| and |channel_size| describe the float**
  // |audio_samples|. |audio_samples| is assumed to point to a
  // two-dimensional |num_channels * channel_size| array of floats.
  FloatAudioFrame(const float* const* audio_samples,
                  size_t num_channels,
                  size_t channel_size)
      : audio_samples_(audio_samples),
        num_channels_(num_channels),
        channel_size_(channel_size) {}

  // Note: implicit!
  FloatAudioFrame(const MutableFloatAudioFrame& mutable_float_frame)  // NOLINT
      : audio_samples_(mutable_float_frame.audio_samples_),
        num_channels_(mutable_float_frame.num_channels_),
        channel_size_(mutable_float_frame.channel_size_) {}

  FloatAudioFrame() = delete;

  size_t num_channels() const { return num_channels_; }

  size_t samples_per_channel() const { return channel_size_; }

  rtc::ArrayView<const float> channel(size_t idx) const {
    RTC_DCHECK_LE(0, idx);
    RTC_DCHECK_LE(idx, num_channels_);
    return rtc::ArrayView<const float>(audio_samples_[idx], channel_size_);
  }

 private:
  const float* const* audio_samples_;
  size_t num_channels_;
  size_t channel_size_;
};

#endif  // MODULES_AUDIO_PROCESSING_INCLUDE_FLOAT_AUDIO_FRAME_H_
