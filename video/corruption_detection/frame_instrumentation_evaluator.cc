/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_instrumentation_evaluator.h"

#include <cstddef>
#include <optional>
#include <vector>

#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "common_video/corruption_detection_message.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "video/corruption_detection/corruption_classifier.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {

namespace {

int GetSequenceIndex(int old_sequence_index,
                     int sequence_index_update,
                     bool update_the_most_significant_bits) {
  RTC_CHECK_GE(old_sequence_index, 0)
      << "old_sequence_index must not be negative";
  RTC_CHECK_LE(old_sequence_index, 0x7FFF)
      << "old_sequence_index must be at most 15 bits";
  RTC_CHECK_GE(sequence_index_update, 0)
      << "sequence_index_update must not be negative";
  RTC_CHECK_LE(sequence_index_update, 0b0111'1111)
      << "sequence_index_update must be at most 7 bits";
  if (update_the_most_significant_bits) {
    // Synchronize index: LSB resets to 0.
    return sequence_index_update << 7;
  }
  int upper_bits = old_sequence_index & 0b0011'1111'1000'0000;
  if (sequence_index_update < (old_sequence_index & 0b0111'1111)) {
    // Assume one and only one wraparound has happened.
    upper_bits += 0b1000'0000;
  }
  // Replace the lowest bits with the bits from the update.
  return upper_bits + sequence_index_update;
}

std::vector<FilteredSample> ConvertSampleValuesToFilteredSamples(
    rtc::ArrayView<const double> values,
    rtc::ArrayView<const FilteredSample> samples) {
  RTC_CHECK_EQ(values.size(), samples.size())
      << "values and samples must have the same size";
  std::vector<FilteredSample> filtered_samples;
  filtered_samples.reserve(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    filtered_samples.push_back({.value = values[i], .plane = samples[i].plane});
  }
  return filtered_samples;
}

}  // namespace

std::optional<double> FrameInstrumentationEvaluator::GetCorruptionScore(
    CorruptionDetectionMessage message,
    VideoFrame frame) {
  frame_sampler_.SetCurrentIndex(GetSequenceIndex(
      frame_sampler_.GetCurrentIndex(), message.sequence_index(),
      message.interpret_sequence_index_as_most_significant_bits()));
  if (message.sample_values().empty()) {
    // This is just a sync message.
    return std::nullopt;
  }

  scoped_refptr<I420BufferInterface> frame_buffer_as_i420 =
      frame.video_frame_buffer()->ToI420();
  if (!frame_buffer_as_i420) {
    RTC_LOG(LS_ERROR) << "Failed to convert "
                      << VideoFrameBufferTypeToString(
                             frame.video_frame_buffer()->type())
                      << " image to I420";
    return std::nullopt;
  }

  std::vector<HaltonFrameSampler::Coordinates> sample_coordinates =
      frame_sampler_.GetSampleCoordinatesForFrame(
          message.sample_values().size());

  std::vector<FilteredSample> samples =
      GetSampleValuesForFrame(frame_buffer_as_i420, sample_coordinates,
                              frame.width(), frame.height(), message.std_dev());
  if (samples.empty()) {
    RTC_LOG(LS_ERROR) << "Failed to get sample values for frame";
    return std::nullopt;
  }

  std::vector<FilteredSample> message_samples =
      ConvertSampleValuesToFilteredSamples(message.sample_values(), samples);

  // TODO: bugs.webrtc.org/358039777 - Update before rollout. Which variant of
  // classifier should we use? What input parameters should it have?
  CorruptionClassifier classifier(2.5);

  return classifier.CalculateCorruptionProbablility(
      message_samples, samples, message.luma_error_threshold(),
      message.chroma_error_threshold());
}

}  // namespace webrtc
