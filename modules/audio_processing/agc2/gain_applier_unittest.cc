/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/gain_applier.h"

#include <math.h>

#include <algorithm>
#include <limits>
#include <tuple>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kMinS16Value =
    static_cast<float>(std::numeric_limits<int16_t>::min());
constexpr float kMaxS16Value =
    static_cast<float>(std::numeric_limits<int16_t>::max());

// Tolerance for EXPECT_NEAR calls when original and processed samples are
// compared.
constexpr float kS16SampleTolerance = 0.1f;

class GainApplierParametrization
    : public ::testing::TestWithParam<std::tuple<int, int>> {
 protected:
  int sample_rate_hz() const { return std::get<0>(GetParam()); }
  int num_channels() const { return std::get<1>(GetParam()); }
  // Returns the number of samples for a 10 ms frame.
  int SamplesPerChannel() const { return sample_rate_hz() / 100; }
};

// Checks that the gain specified when `GainApplier` is constructed is applied
// when hard clipping is enabled.
TEST_P(GainApplierParametrization, InitialGainIsAppliedWithHardClipping) {
  constexpr float kGainDb = 20.0f;
  GainApplier gain_applier(kGainDb, /*hard_clip=*/true, sample_rate_hz());
  constexpr float kStartValue = 123.0f;
  VectorFloatFrame frame(num_channels(), SamplesPerChannel(), kStartValue);
  gain_applier.ApplyGain(frame.float_frame_view());

  const float gain_factor = DbToRatio(kGainDb);
  for (int c = 0; c < num_channels(); ++c) {
    SCOPED_TRACE(c);
    EXPECT_NEAR(frame.float_frame_view().channel(c)[0],
                kStartValue * gain_factor, kS16SampleTolerance);
  }
}

// Checks that the gain specified when `GainApplier` is constructed is applied
// when hard clipping is disabled.
TEST_P(GainApplierParametrization, InitialGainIsAppliedWithoutHardClipping) {
  constexpr float kGainDb = 20.0f;
  GainApplier gain_applier(kGainDb, /*hard_clip=*/false, sample_rate_hz());
  constexpr float kStartValue = 123.0f;
  VectorFloatFrame frame(num_channels(), SamplesPerChannel(), kStartValue);
  gain_applier.ApplyGain(frame.float_frame_view());

  const float gain_factor = DbToRatio(kGainDb);
  for (int c = 0; c < num_channels(); ++c) {
    SCOPED_TRACE(c);
    EXPECT_NEAR(frame.float_frame_view().channel(c)[0],
                kStartValue * gain_factor, kS16SampleTolerance);
  }
}

// Checks that hard clipping is not applied when disabled.
TEST_P(GainApplierParametrization, HardClippingNotApplied) {
  constexpr float kGainDb = 20.0f;
  GainApplier gain_applier(kGainDb, /*hard_clip=*/false, sample_rate_hz());
  constexpr float kStartValue = 30'000.0f;
  VectorFloatFrame frame(num_channels(), SamplesPerChannel(), kStartValue);
  gain_applier.ApplyGain(frame.float_frame_view());

  const float gain_factor = DbToRatio(kGainDb);
  for (int c = 0; c < num_channels(); ++c) {
    SCOPED_TRACE(c);
    EXPECT_NEAR(frame.float_frame_view().channel(c)[0],
                kStartValue * gain_factor, kS16SampleTolerance);
  }
}

// Checks that hard clipping is not applied when disabled.
TEST_P(GainApplierParametrization, HardClippingApplied) {
  constexpr float kGainDb = 20.0f;
  GainApplier gain_applier(kGainDb, /*hard_clip=*/true, sample_rate_hz());

  // Positive samples.
  VectorFloatFrame frame0(num_channels(), SamplesPerChannel(),
                          /*start_value=*/30'000.0f);
  gain_applier.ApplyGain(frame0.float_frame_view());
  for (int c = 0; c < num_channels(); ++c) {
    SCOPED_TRACE(c);
    EXPECT_EQ(frame0.float_frame_view().channel(c)[0], kMaxS16Value);
  }

  // Negative samples.
  VectorFloatFrame frame1(num_channels(), SamplesPerChannel(),
                          /*start_value=*/-30'000.0f);
  gain_applier.ApplyGain(frame1.float_frame_view());
  for (int c = 0; c < num_channels(); ++c) {
    SCOPED_TRACE(c);
    EXPECT_EQ(frame1.float_frame_view().channel(c)[0], kMinS16Value);
  }
}

TEST_P(GainApplierParametrization, RampUpAfterGainInrease) {
  constexpr float kGainDb = 10.0f;
  GainApplier gain_applier(kGainDb, /*hard_clip=*/true, sample_rate_hz());
  constexpr float kStartValue = 123.0f;
  // Process one frame with the initial gain.
  VectorFloatFrame frame0(num_channels(), SamplesPerChannel(), kStartValue);
  gain_applier.ApplyGain(frame0.float_frame_view());
  // Increase the gain and process another frame.
  gain_applier.SetGainDb(kGainDb * 2.0f);
  VectorFloatFrame frame1(num_channels(), SamplesPerChannel(), kStartValue);
  gain_applier.ApplyGain(frame1.float_frame_view());

  // Check that every channel starts at the input level and applies increasingly
  // monotonic gains.
  auto view = frame1.float_frame_view();
  for (int c = 0; c < num_channels(); ++c) {
    SCOPED_TRACE(c);
    float previous =
        frame0.float_frame_view().channel(c)[SamplesPerChannel() - 1];
    EXPECT_EQ(view.channel(c)[0], previous);
    for (int i = 1; i < SamplesPerChannel(); ++i) {
      SCOPED_TRACE(i);
      EXPECT_LT(previous, view.channel(c)[i]);
      previous = view.channel(c)[i];
    }
  }
}

TEST_P(GainApplierParametrization, RampDownAfterGainDecrease) {
  constexpr float kGainDb = 10.0f;
  GainApplier gain_applier(kGainDb, /*hard_clip=*/true, sample_rate_hz());
  constexpr float kStartValue = 123.0f;
  // Process one frame with the initial gain.
  VectorFloatFrame frame0(num_channels(), SamplesPerChannel(), kStartValue);
  gain_applier.ApplyGain(frame0.float_frame_view());

  // Decrease the gain and process another frame.
  gain_applier.SetGainDb(kGainDb / 2.0f);
  VectorFloatFrame frame1(num_channels(), SamplesPerChannel(), kStartValue);
  gain_applier.ApplyGain(frame1.float_frame_view());

  // Check that every channel starts at the input level and applies increasingly
  // monotonic gains.
  auto view = frame1.float_frame_view();
  for (int c = 0; c < num_channels(); ++c) {
    SCOPED_TRACE(c);
    float previous =
        frame0.float_frame_view().channel(c)[SamplesPerChannel() - 1];
    EXPECT_EQ(view.channel(c)[0], previous);
    for (int i = 1; i < SamplesPerChannel(); ++i) {
      SCOPED_TRACE(i);
      EXPECT_GT(previous, view.channel(c)[i]);
      previous = view.channel(c)[i];
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    GainController2GainApplier,
    GainApplierParametrization,
    ::testing::Combine(::testing::Values(8000, 16000, 32000, 44100, 48000),
                       ::testing::Values(1, 2, 5)));

}  // namespace
}  // namespace webrtc
