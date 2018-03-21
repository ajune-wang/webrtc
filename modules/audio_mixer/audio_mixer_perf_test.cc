/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <iostream>
#include <vector>

#include "api/audio/audio_mixer.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_mixer/default_output_rate_calculator.h"
#include "rtc_base/flags.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {

constexpr float kMaxInt16 =
    static_cast<float>(std::numeric_limits<int16_t>::max());

constexpr std::array<std::array<float, 3>, 8> kStreams = {{
    {{1.f / 479.f, 0.8f * kMaxInt16, 48000}},   // Changes fast
    {{1.f / 4799.f, 0.8f * kMaxInt16, 48000}},  // Changes slow
    {{1.f / 3.f, 0.8f * kMaxInt16, 48000}},     // Changes very fast
    {{1.f / 3.f, 0.05f * kMaxInt16,
      48000}},  // Changes very fast and is not loud.

    {{1.f / 163.f, 0.8f * kMaxInt16, 16000}},   // Changes fast
    {{1.f / 1601.f, 0.8f * kMaxInt16, 16000}},  // Changes slow
    {{1.f / 3.f, 0.8f * kMaxInt16, 16000}},     // Changes very fast
    {{1.f / 3.f, 0.05f * kMaxInt16,
      16000}},  // Changes very fast and is not loud.
}};

class SineSource : public AudioMixer::Source {
 public:
  // Freq is in measured in radians / sample. This is for PROFILING,
  // not to produce a perfect sine tone. We optimize 'time to write
  // the code' here.
  explicit SineSource(float freq1, float amplitude, int sample_rate_hz)
      : freq1_(freq1),
        freq2_(freq1_ * 43.f / 41.f),
        amplitude_(amplitude),
        sample_rate_hz_(sample_rate_hz),
        samples_per_channel_(sample_rate_hz_ / 100),
        number_of_channels_(2) {}

  AudioFrameInfo GetAudioFrameWithInfo(int target_rate_hz,
                                       AudioFrame* frame) override {
    RTC_DCHECK_EQ(target_rate_hz, sample_rate_hz_);
    frame->samples_per_channel_ = samples_per_channel_;
    frame->num_channels_ = number_of_channels_;
    frame->sample_rate_hz_ = target_rate_hz;

    std::copy(frame_.begin(),
              frame_.begin() + samples_per_channel_ * number_of_channels_,
              frame->mutable_data());
    return AudioFrameInfo::kNormal;
  }

  void PrepareFrame() {
    const float f1_change = 1.f / freq1_;
    const float f2_change = 1.f / freq2_;

    for (int i = 0; i < samples_per_channel_; ++i) {
      frame_[number_of_channels_ * i] =
          static_cast<int16_t>(std::sin(pos_1_in_period_) * amplitude_);
      if (number_of_channels_ == 2) {
        frame_[number_of_channels_ * i + 1] =
            static_cast<int16_t>(std::sin(pos_2_in_period_) * amplitude_);
      }
      pos_1_in_period_ += f1_change;
      pos_2_in_period_ += f2_change;
    }
  }

  int Ssrc() const override { return 0; }
  int PreferredSampleRate() const override { return sample_rate_hz_; }

 private:
  float pos_1_in_period_ = 0.f;
  float pos_2_in_period_ = 0.f;
  float freq1_;
  float freq2_;
  float amplitude_;
  int sample_rate_hz_;
  int samples_per_channel_;
  int number_of_channels_;
  std::array<int16_t, 2 * 480> frame_;
};

class TestTimer {
 public:
  explicit TestTimer(size_t num_values_to_store)
      : clock_(webrtc::Clock::GetRealTimeClock()) {
    timestamps_.resize(num_values_to_store);
  }

  void ResetTimer() { start_timestamp_ = clock_->TimeInMicroseconds(); }
  void AddTimeStamp() {
    RTC_CHECK_LE(num_timestamps_stored_, timestamps_.size());
    timestamps_[num_timestamps_stored_] =
        clock_->TimeInMicroseconds() - start_timestamp_;
    ++num_timestamps_stored_;
  }

  double GetDurationAverage() const {
    RTC_DCHECK_EQ(num_timestamps_stored_, timestamps_.size());
    int64_t durations_sum = 0;
    for (size_t k = kNumTimestampsToExclude; k < timestamps_.size(); k++) {
      durations_sum += timestamps_[k];
    }

    RTC_DCHECK_LT(kNumTimestampsToExclude, timestamps_.size());
    return static_cast<double>(durations_sum) /
           (timestamps_.size() - kNumTimestampsToExclude);
  }

  double GetDurationStandardDeviationGetVarianceTime() const {
    int32_t average_duration = GetDurationAverage();
    int64_t variance = 0;
    for (size_t k = kNumTimestampsToExclude; k < timestamps_.size(); k++) {
      variance += timestamps_[k] - average_duration;
    }

    RTC_DCHECK_LT(kNumTimestampsToExclude, timestamps_.size());
    return sqrt(static_cast<double>(variance) /
                (timestamps_.size() - kNumTimestampsToExclude));
  }

 private:
  const size_t kNumTimestampsToExclude = 10u;
  webrtc::Clock* clock_;
  int64_t start_timestamp_ = 0;
  size_t num_timestamps_stored_ = 0;
  std::vector<int64_t> timestamps_;
};

void RunMixer(int sample_rate_hz,
              rtc::ArrayView<SineSource> sources,
              bool new_limiter,
              std::string desc) {
  rtc::scoped_refptr<AudioMixerImpl> mixer(AudioMixerImpl::Create(
      std::unique_ptr<OutputRateCalculator>(new DefaultOutputRateCalculator()),
      false));
  mixer->SetLimiterType(static_cast<FrameCombiner::LimiterType>(
      new_limiter ? FrameCombiner::LimiterType::kApmAgc2Limiter
                  : FrameCombiner::LimiterType::kApmAgcLimiter));

  for (auto& sine_source : sources) {
    mixer->AddSource(&sine_source);
  }

  constexpr int kNumFramesToProcess = 10000;
  TestTimer timer(kNumFramesToProcess);
  AudioFrame mix_frame;

  for (int i = 0; i < kNumFramesToProcess; ++i) {
    for (auto& sine_source : sources) {
      sine_source.PrepareFrame();
    }
    timer.ResetTimer();
    mixer->Mix(2, &mix_frame);
    timer.AddTimeStamp();
  }

  std::string description = (new_limiter ? "Agc2-" : "Agc1-") + desc;

  webrtc::test::PrintResultMeanAndError(
      "apm_submodule_call_durations",
      "_" + std::to_string(sample_rate_hz) + "Hz", description,
      timer.GetDurationAverage(),  // MEAN
      timer.GetDurationStandardDeviationGetVarianceTime(), "us", false);
}

// const int kNativeSampleRatesHz[] = {8000, 16000, 32000, 48000};

TEST(AudioMixerPerfTest, PerfTest) {
  for (bool use_new : {true, false}) {
    std::vector<SineSource> streams;

    streams.clear();
    const auto& not_loud_48 = kStreams[3];
    streams.emplace_back(not_loud_48[0], not_loud_48[1], not_loud_48[2]);
    streams.emplace_back(not_loud_48[0], not_loud_48[1], not_loud_48[2]);
    RunMixer(48000, streams, use_new, "kIdentity");

    streams.clear();
    const auto& not_loud_16 = kStreams[7];
    streams.emplace_back(not_loud_16[0], not_loud_16[1], not_loud_16[2]);
    streams.emplace_back(not_loud_16[0], not_loud_16[1], not_loud_16[2]);
    RunMixer(16000, streams, use_new, "kIdentity");

    streams.clear();
    const auto& loud_48 = kStreams[0];
    const auto& loud_slow_48 = kStreams[1];
    streams.emplace_back(loud_48[0], loud_48[1], loud_48[2]);
    streams.emplace_back(loud_slow_48[0], loud_slow_48[1], loud_slow_48[2]);
    RunMixer(48000, streams, use_new, "above identity");

    streams.clear();
    const auto& loud_fast_48 = kStreams[2];
    streams.emplace_back(loud_48[0], loud_48[1], loud_48[2]);
    streams.emplace_back(loud_slow_48[0], loud_slow_48[1], loud_slow_48[2]);
    streams.emplace_back(loud_fast_48[0], loud_fast_48[1], loud_fast_48[2]);
    RunMixer(48000, streams, use_new, "constant high");

    streams.clear();
    const auto& loud_16 = kStreams[4 + 0];
    const auto& loud_slow_16 = kStreams[4 + 1];
    streams.emplace_back(loud_16[0], loud_16[1], loud_16[2]);
    streams.emplace_back(loud_slow_16[0], loud_slow_16[1], loud_slow_16[2]);
    RunMixer(16000, streams, use_new, "above identity");

    streams.clear();
    const auto& loud_fast_16 = kStreams[4 + 2];
    streams.emplace_back(loud_16[0], loud_16[1], loud_16[2]);
    streams.emplace_back(loud_slow_16[0], loud_slow_16[1], loud_slow_16[2]);
    streams.emplace_back(loud_fast_16[0], loud_fast_16[1], loud_fast_16[2]);
    RunMixer(16000, streams, use_new, "constant high");
  }
}

}  // namespace webrtc
