/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_resampler.h"

#include <string.h>

#include "rtc_base/logging.h"

namespace webrtc {
namespace acm2 {

ACMResampler::ACMResampler() {}

ACMResampler::~ACMResampler() {}

int ACMResampler::Resample10Msec(rtc::ArrayView<const int16_t> in_audio,
                                 int in_freq_hz,
                                 int out_freq_hz,
                                 size_t num_audio_channels,
                                 rtc::ArrayView<int16_t> out_audio) {
  RTC_DCHECK_EQ(in_audio.size(), (in_freq_hz / 100) * num_audio_channels)
      << "freq=" << in_freq_hz << " channels=" << num_audio_channels;
  if (in_freq_hz == out_freq_hz) {
    if (out_audio.size() < in_audio.size()) {
      RTC_DCHECK_NOTREACHED();
      return -1;
    }
    memcpy(out_audio.data(), in_audio.data(),
           in_audio.size() * sizeof(int16_t));
    return static_cast<int>(in_audio.size() / num_audio_channels);
  }

  if (resampler_.InitializeIfNeeded(in_freq_hz, out_freq_hz,
                                    num_audio_channels) != 0) {
    RTC_LOG(LS_ERROR) << "InitializeIfNeeded(" << in_freq_hz << ", "
                      << out_freq_hz << ", " << num_audio_channels
                      << ") failed.";
    return -1;
  }

  int out_length = resampler_.Resample(in_audio, out_audio);
  if (out_length == -1) {
    RTC_LOG(LS_ERROR) << "Resample failed. in_audio.size=" << in_audio.size()
                      << ", out_audio.size=" << out_audio.size();
    return -1;
  }

  return static_cast<int>(out_length / num_audio_channels);
}

}  // namespace acm2
}  // namespace webrtc
