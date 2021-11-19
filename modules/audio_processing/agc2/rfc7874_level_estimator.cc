/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rfc7874_level_estimator.h"

#include <algorithm>
#include <cmath>

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// Coefficients of high pass and low pass bi-quad filters for different sample
// rates obtained as follows:
//
// import scipy.signal
//
// def PrintFilterConfig(sample_rate_hz, b, a, name):
//   sample_rate_khz = sample_rate_hz // 1000
//   print(f"constexpr BiQuadFilter::Config k{name}{sample_rate_khz}kHz{{\n ",
//         f"{{{b[0]}f, {b[1]}f, {b[2]}f}},\n ",
//         f"{{{a[1]}f, {a[2]}f}}}};")
//
//
// f0_hz = 300
// for sample_rate_hz in [8000, 16000, 32000, 48000]:
//   [b, a] = signal.butter(N=2, Wn=f0_hz, btype="highpass", fs=sample_rate_hz)
//   PrintFilterConfig(sample_rate_hz, b, a, "HighPass")

constexpr BiQuadFilter::Config kHighPass8kHz{
    {0.84645927f, -1.692918539f, 0.84645927f},
    {-1.669203162f, 0.716633856f}};
constexpr BiQuadFilter::Config kHighPass16kHz{
    {0.920066178f, -1.840132356f, 0.920066178f},
    {-1.833732605f, 0.846531987f}};
constexpr BiQuadFilter::Config kHighPass32kHz{
    {0.959203124f, -1.918406248f, 0.959203124f},
    {-1.916741252f, 0.920071363f}};
constexpr BiQuadFilter::Config kHighPass48kHz{
    {0.972613871f, -1.945227742f, 0.972613871f},
    {-1.944477677f, 0.945977926f}};

BiQuadFilter::Config GetHighPassConfig(int sample_rate_hz) {
  switch (sample_rate_hz) {
    case 8000:
      return kHighPass8kHz;
    case 16000:
      return kHighPass16kHz;
    case 32000:
      return kHighPass32kHz;
    case 48000:
      return kHighPass48kHz;
    default:
      RTC_DCHECK_NOTREACHED();
      return {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}};  // All pass.
  }
}

}  // namespace

Rfc7874AudioLevelEstimator::Rfc7874AudioLevelEstimator(
    int sample_rate_hz,
    ApmDataDumper* apm_data_dumper)
    : apm_data_dumper_(apm_data_dumper),
      buffer_(rtc::CheckedDivExact(sample_rate_hz, 100)),
      high_pass_filter_(GetHighPassConfig(sample_rate_hz)) {
  RTC_DCHECK(apm_data_dumper);
}

void Rfc7874AudioLevelEstimator::Initialize(int sample_rate_hz) {
  buffer_.resize(rtc::CheckedDivExact(sample_rate_hz, 100));
  high_pass_filter_.SetConfig(GetHighPassConfig(sample_rate_hz));
}

Rfc7874AudioLevelEstimator::Levels Rfc7874AudioLevelEstimator::GetLevels(
    rtc::ArrayView<const float> audio) {
  RTC_DCHECK_EQ(audio.size(), buffer_.size());
  high_pass_filter_.Process(audio, buffer_);
  apm_data_dumper_->DumpWav("agc2_rfc7848_filtered_audio", buffer_,
                            /*sample_rate_hz=*/buffer_.size() * 100,
                            /*num_channels=*/1);
  float peak = 0.0f;
  float energy = 0.0f;
  for (const auto& x : buffer_) {
    peak = std::max(std::fabs(x), peak);
    energy += x * x;
  }
  return {peak, energy};
}

}  // namespace webrtc
