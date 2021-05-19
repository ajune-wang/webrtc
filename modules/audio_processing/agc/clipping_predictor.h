/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_H_
#define MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

// A circular buffer to store frame-wise |Level| items (square average and peak
// value) for clipping prediction.
class LevelBuffer {
 public:
  struct Level {
    float average;
    float max;
  };

  explicit LevelBuffer(size_t buffer_max_length)
      : buffer_max_length_(buffer_max_length), tail_(-1) {}
  ~LevelBuffer() {}
  void Reset() {
    data_.clear();
    tail_ = -1;
  }
  void Initialize();
  size_t Size() const { return data_.size(); }

  // Adds |level| into the circular buffer |data_|. Stores at most
  // |buffer_max_length_| items. If more items are added, adding a new item
  // replaces the least recently added item.
  void Push(Level level);

  // If enough |Level| items have been stored, returns the partial average for
  // |num_items| frames at index |delay| and before.
  absl::optional<float> ComputePartialAverage(size_t delay,
                                              size_t num_items) const;

  // If enough |Level| items have been stored, returns the partial maximum
  // for |num_items| values at index |delay| and before.
  absl::optional<float> ComputePartialMax(size_t delay, size_t num_items) const;

  // Returns the average for a non-empty buffer.
  absl::optional<float> ComputeAverage() const;

  // Returns the maximum value for a non-empty buffer.
  absl::optional<float> ComputeMax() const;

 private:
  const size_t buffer_max_length_;
  int tail_;
  std::vector<Level> data_;
};

// Frame-wise clipping prediction. Procssing in two steps: |ProcessAudioFrame|
// analyses a frame of audio and stores the intermediate |Level| metrics for
// later clipping prediction whereas |PredictClippingEvent| and
// |ProjectClippedPeakValue| perform the clipping prediction. The frame metrics
// from processing are stored for at most |buffered_levels_| +
// |buffered_previous_levels_| frames at each time.
class ClippingPredictor {
 public:
  ClippingPredictor(size_t num_channels,
                    size_t buffered_levels,
                    size_t previous_buffered_levels,
                    int clipping_threshold,
                    int crest_factor_margin);
  ~ClippingPredictor() {}

  void Reset();
  void Initialize();
  size_t GetBufferSize() const {
    return buffered_levels_ + previous_buffered_levels_;
  }

  // Processes a frame of audio and stores the channel-wise averages of squared
  // values and maximum absolute values in the circular buffers in
  // |ch_buffers_|. Each buffer has a maximum size |buffered_levels_| +
  // |buffered_previous_levels_|. Once a buffer is full, processing a new frame
  // replaces values stored earlier.
  bool ProcessAudioFrame(const float* const* audio,
                         size_t num_channels,
                         size_t samples_per_channel);

  // Performs crest factor-based clipping prediction using the buffered values
  // in |ch_buffers_|. The minimum difference between the earlier and most
  // recent crest factors is given in |crest_factor_margin| and the activation
  // threshold in |clipping_threshold_|. Prediction is only performed if enough
  // audio frames have been successfully processed by ProcessAudioFrame().
  // Returns true if clipping event is predicted.
  bool PredictClippingEvent(size_t channel) const;

  // Performs crest-factor based clipped level estimation from the past crest
  // factor and recent RMS value using the buffered values in |ch_buffers_|.
  // Returns the estimated clipped level if the peak value exceeds
  // |clipping_threshold_| and if enough audio frames have been successfully
  // processed by ProcessAudioFrame().
  absl::optional<float> ProjectClippedPeakValue(size_t channel) const;

 private:
  // If enough framees available, computes the ratio of the frame peak and root
  // mean square values for |num_frames| frames from the |delay|th most recent
  // frame backwards.
  absl::optional<float> ComputeCrestFactor(size_t channel,
                                           size_t delay,
                                           size_t num_frames) const;

  // Stores the channel and framewise mean square and peak value for the
  // |buffered_levels| + |previous_buffered_levels_| most recent frame. If a
  // buffer is full, processing new frames replaces the least recent values.
  std::vector<std::unique_ptr<LevelBuffer>> ch_buffers_;
  const size_t buffered_levels_;
  const size_t previous_buffered_levels_;

  // Clipping prediction activation threshold in dB. Only peak values higher
  // than |prediction_threshold| can result in clipping prediction. For instance
  // value -1 refers to 1dB drop from the full range.
  const int clipping_threshold_;
  // Minimum crest factor drop that can result in clipping event prediction. No
  // effect on clipped level estimation.
  const int crest_factor_margin_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_H_
