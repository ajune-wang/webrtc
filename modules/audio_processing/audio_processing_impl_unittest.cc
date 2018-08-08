/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/audio_processing_impl.h"

#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Invoke;

namespace webrtc {
namespace {

class MockInitialize : public AudioProcessingImpl {
 public:
  explicit MockInitialize(const webrtc::Config& config)
      : AudioProcessingImpl(config) {}

  MOCK_METHOD0(InitializeLocked, int());
  int RealInitializeLocked() RTC_NO_THREAD_SAFETY_ANALYSIS {
    return AudioProcessingImpl::InitializeLocked();
  }

  MOCK_CONST_METHOD0(AddRef, void());
  MOCK_CONST_METHOD0(Release, rtc::RefCountReleaseStatus());
};

void InitializeAudioFrame(size_t input_rate,
                          size_t num_channels,
                          AudioFrame* frame) {
  const size_t samples_per_input_channel = rtc::CheckedDivExact(
      input_rate, static_cast<size_t>(rtc::CheckedDivExact(
                      1000, AudioProcessing::kChunkSizeMs)));
  RTC_DCHECK_LE(samples_per_input_channel * num_channels,
                AudioFrame::kMaxDataSizeSamples);
  frame->samples_per_channel_ = samples_per_input_channel;
  frame->sample_rate_hz_ = input_rate;
  frame->num_channels_ = num_channels;
}

void FillFixedFrame(int16_t audio_level, AudioFrame* frame) {
  const size_t num_samples = frame->samples_per_channel_ * frame->num_channels_;
  for (size_t i = 0; i < num_samples; ++i) {
    frame->mutable_data()[i] = audio_level;
  }
}

class TestEchoDetector : public EchoDetector {
 public:
  TestEchoDetector()
      : analyze_render_audio_called_(false),
        last_render_audio_first_sample_(0.f) {}
  ~TestEchoDetector() override = default;
  void AnalyzeRenderAudio(rtc::ArrayView<const float> render_audio) override {
    last_render_audio_first_sample_ = render_audio[0];
    analyze_render_audio_called_ = true;
  }
  void AnalyzeCaptureAudio(rtc::ArrayView<const float> capture_audio) override {
  }
  void Initialize(int capture_sample_rate_hz,
                  int num_capture_channels,
                  int render_sample_rate_hz,
                  int num_render_channels) override {}
  EchoDetector::Metrics GetMetrics() const override { return {}; }
  // Returns true if AnalyzeRenderAudio() has been called at least once.
  bool analyze_render_audio_called() const {
    return analyze_render_audio_called_;
  }
  // Returns the first sample of the last analyzed render frame.
  float last_render_audio_first_sample() const {
    return last_render_audio_first_sample_;
  }

 private:
  bool analyze_render_audio_called_;
  float last_render_audio_first_sample_;
};

class TestRenderPreProcessor : public CustomProcessing {
 public:
  TestRenderPreProcessor() = default;
  ~TestRenderPreProcessor() = default;
  void Initialize(int sample_rate_hz, int num_channels) override {}
  void Process(AudioBuffer* audio) override {
    for (size_t k = 0; k < audio->num_channels(); ++k) {
      rtc::ArrayView<float> channel_view(audio->channels_f()[k],
                                         audio->num_frames());
      std::transform(channel_view.begin(), channel_view.end(),
                     channel_view.begin(), ProcessSample);
    }
  };
  std::string ToString() const override { return "TestRenderPreProcessor"; }
  void SetRuntimeSetting(AudioProcessing::RuntimeSetting setting) override {}
  // Modifies a sample. This member is used in Process() to modify a frame and
  // it is publicly visible to enable tests.
  static float ProcessSample(float x) { return 2.f * x; }
};

}  // namespace

TEST(AudioProcessingImplTest, AudioParameterChangeTriggersInit) {
  webrtc::Config config;
  MockInitialize mock(config);
  ON_CALL(mock, InitializeLocked())
      .WillByDefault(Invoke(&mock, &MockInitialize::RealInitializeLocked));

  EXPECT_CALL(mock, InitializeLocked()).Times(1);
  mock.Initialize();

  AudioFrame frame;
  // Call with the default parameters; there should be an init.
  frame.num_channels_ = 1;
  SetFrameSampleRate(&frame, 16000);
  EXPECT_CALL(mock, InitializeLocked()).Times(0);
  EXPECT_NOERR(mock.ProcessStream(&frame));
  EXPECT_NOERR(mock.ProcessReverseStream(&frame));

  // New sample rate. (Only impacts ProcessStream).
  SetFrameSampleRate(&frame, 32000);
  EXPECT_CALL(mock, InitializeLocked()).Times(1);
  EXPECT_NOERR(mock.ProcessStream(&frame));

  // New number of channels.
  // TODO(peah): Investigate why this causes 2 inits.
  frame.num_channels_ = 2;
  EXPECT_CALL(mock, InitializeLocked()).Times(2);
  EXPECT_NOERR(mock.ProcessStream(&frame));
  // ProcessStream sets num_channels_ == num_output_channels.
  frame.num_channels_ = 2;
  EXPECT_NOERR(mock.ProcessReverseStream(&frame));

  // A new sample rate passed to ProcessReverseStream should cause an init.
  SetFrameSampleRate(&frame, 16000);
  EXPECT_CALL(mock, InitializeLocked()).Times(1);
  EXPECT_NOERR(mock.ProcessReverseStream(&frame));
}

TEST(AudioProcessingImplTest, UpdateCapturePreGainRuntimeSetting) {
  std::unique_ptr<AudioProcessing> apm(AudioProcessingBuilder().Create());
  webrtc::AudioProcessing::Config apm_config;
  apm_config.pre_amplifier.enabled = true;
  apm_config.pre_amplifier.fixed_gain_factor = 1.f;
  apm->ApplyConfig(apm_config);

  AudioFrame frame;
  constexpr int16_t audio_level = 10000;
  constexpr size_t input_rate = 48000;
  constexpr size_t num_channels = 2;

  InitializeAudioFrame(input_rate, num_channels, &frame);
  FillFixedFrame(audio_level, &frame);
  apm->ProcessStream(&frame);
  EXPECT_EQ(frame.data()[100], audio_level)
      << "With factor 1, frame shouldn't be modified.";

  constexpr float gain_factor = 2.f;
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePreGain(gain_factor));

  // Process for two frames to have time to ramp up gain.
  for (int i = 0; i < 2; ++i) {
    FillFixedFrame(audio_level, &frame);
    apm->ProcessStream(&frame);
  }
  EXPECT_EQ(frame.data()[100], gain_factor * audio_level)
      << "Frame should be amplified.";
}

TEST(AudioProcessingImplTest, RenderPreProcessorBeforeEchoAnalysis) {
  // Make sure that signal changes caused by a render pre-processing sub-module
  // take place before any echo analysis step is performed (e.g., echo
  // detection).
  rtc::scoped_refptr<TestEchoDetector> test_echo_detector(
      new rtc::RefCountedObject<TestEchoDetector>());
  std::unique_ptr<CustomProcessing> test_render_pre_processor(
      new TestRenderPreProcessor());
  // Create APM enabling the
  std::unique_ptr<AudioProcessing> apm(
      AudioProcessingBuilder()
          .SetEchoDetector(test_echo_detector)
          .SetRenderPreProcessing(std::move(test_render_pre_processor))
          .Create());
  webrtc::AudioProcessing::Config apm_config;
  apm_config.pre_amplifier.enabled = true;
  apm_config.residual_echo_detector.enabled = true;
  apm->ApplyConfig(apm_config);

  AudioFrame frame;
  constexpr int16_t audio_level = 1000;
  constexpr int sample_rate_hz = 16000;
  constexpr size_t num_channels = 1;
  InitializeAudioFrame(sample_rate_hz, num_channels, &frame);

  constexpr float audio_level_f = static_cast<float>(audio_level);
  const float expected_preprocessed_audio_level =
      TestRenderPreProcessor::ProcessSample(audio_level_f);
  ASSERT_NE(audio_level_f, expected_preprocessed_audio_level);

  // Analyze the same reverse stream frame until AnalyzeRenderAudio() is called.
  constexpr size_t kMaxAnalyzeReverseStreamCalls = 1000;
  for (size_t i = 0; i < kMaxAnalyzeReverseStreamCalls &&
                     !test_echo_detector->analyze_render_audio_called();
       ++i) {
    FillFixedFrame(audio_level, &frame);
    ASSERT_EQ(AudioProcessing::Error::kNoError,
              apm->ProcessReverseStream(&frame));
  }
  ASSERT_TRUE(test_echo_detector->analyze_render_audio_called())
      << "Try choosing a larger value for |kMaxAnalyzeReverseStreamCalls|.";
  EXPECT_EQ(expected_preprocessed_audio_level,
            test_echo_detector->last_render_audio_first_sample());
}

}  // namespace webrtc
