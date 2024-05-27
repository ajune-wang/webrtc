/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/resampler/include/push_resampler.h"

#include <stdint.h>
#include <string.h>

#include <memory>

#include "api/audio/audio_frame.h"
#include "common_audio/include/audio_util.h"
#include "common_audio/resampler/push_sinc_resampler.h"
#include "rtc_base/checks.h"

namespace webrtc {

template <typename T>
PushResampler<T>::PushResampler()
    : src_sample_rate_hz_(0), dst_sample_rate_hz_(0) {}

template <typename T>
PushResampler<T>::~PushResampler() {}

template <typename T>
int PushResampler<T>::InitializeIfNeeded(int src_sample_rate_hz,
                                         int dst_sample_rate_hz,
                                         size_t num_channels) {
  // These checks used to be factored out of this template function due to
  // Windows debug build issues with clang. http://crbug.com/615050
  RTC_DCHECK_GT(src_sample_rate_hz, 0);
  RTC_DCHECK_GT(dst_sample_rate_hz, 0);
  RTC_DCHECK_GT(num_channels, 0);

  if (src_sample_rate_hz == src_sample_rate_hz_ &&
      dst_sample_rate_hz == dst_sample_rate_hz_ &&
      num_channels == source_view_.num_channels()) {
    // No-op if settings haven't changed.
    return 0;
  }

  if (src_sample_rate_hz <= 0 || dst_sample_rate_hz <= 0 || num_channels <= 0) {
    return -1;
  }

  src_sample_rate_hz_ = src_sample_rate_hz;
  dst_sample_rate_hz_ = dst_sample_rate_hz;

  const size_t src_size_10ms_mono =
      static_cast<size_t>(src_sample_rate_hz / 100);
  const size_t dst_size_10ms_mono =
      static_cast<size_t>(dst_sample_rate_hz / 100);

  source_.reset(new T[src_size_10ms_mono * num_channels]);
  destination_.reset(new T[dst_size_10ms_mono * num_channels]);

  // Allocate two buffers for all source and destination channels.
  // Then organize `ChannelResampler` instances for each channel in the buffers.
  source_view_ =
      DeinterleavedView<T>(source_.get(), src_size_10ms_mono, num_channels);
  destination_view_ = DeinterleavedView<T>(destination_.get(),
                                           dst_size_10ms_mono, num_channels);

  channel_resamplers_.clear();
  for (size_t i = 0; i < num_channels; ++i) {
    channel_resamplers_.push_back(ChannelResampler());
    auto channel_resampler = channel_resamplers_.rbegin();
    channel_resampler->resampler = std::make_unique<PushSincResampler>(
        src_size_10ms_mono, dst_size_10ms_mono);
    channel_resampler->source = source_view_[i];
    channel_resampler->destination = destination_view_[i];
  }

  return 0;
}

template <typename T>
int PushResampler<T>::Resample(InterleavedView<const T> src,
                               InterleavedView<T> dst) {
  RTC_DCHECK_EQ(NumChannels(src), NumChannels(source_view_));
  RTC_DCHECK_EQ(NumChannels(dst), NumChannels(source_view_));
  RTC_DCHECK_EQ(SamplesPerChannel(src),
                SampleRateToDefaultChannelSize(src_sample_rate_hz_));
  RTC_DCHECK_EQ(SamplesPerChannel(dst),
                SampleRateToDefaultChannelSize(dst_sample_rate_hz_));

  if (src_sample_rate_hz_ == dst_sample_rate_hz_) {
    // The old resampler provides this memcpy facility in the case of matching
    // sample rates, so reproduce it here for the sinc resampler.
    CopySamples(dst, src);
    return static_cast<int>(src.data().size());
  }

  Deinterleave(src, source_view_);

  for (auto& resampler : channel_resamplers_) {
    size_t dst_length_mono = resampler.resampler->Resample(
        resampler.source.data(), src.samples_per_channel(),
        resampler.destination.data(), dst.samples_per_channel());
    RTC_DCHECK_EQ(dst_length_mono, dst.samples_per_channel());
  }

  Interleave<T>(destination_view_, dst);
  return static_cast<int>(dst.size());
}

// Explictly generate required instantiations.
template class PushResampler<int16_t>;
template class PushResampler<float>;

}  // namespace webrtc
