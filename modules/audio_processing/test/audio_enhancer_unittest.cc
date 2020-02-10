/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>  // size_t

#include <memory>
#include <string>
#include <vector>

#include "api/audio/audio_enhancer.h"
#include "api/audio/echo_canceller3_factory.h"
#include "api/scoped_refptr.h"
#include "modules/audio_coding/neteq/tools/resample_input_audio_file.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/swap_queue.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

namespace {

// Reads a frame of audio data from a file.
void ReadAudioFrame(ResampleInputAudioFile* file,
                    const StreamConfig& config,
                    float* const* buffer) {
  const size_t num_frames = config.num_frames();
  const int channels = config.num_channels();

  std::vector<int16_t> signal(channels * num_frames);

  file->Read(num_frames * channels, signal.data());

  for (int channel = 0; channel < channels; ++channel) {
    for (size_t i = 0; i < num_frames; ++i) {
      buffer[channel][i] = S16ToFloat(signal[i * channels + channel]);
    }
  }
}

// Maintans the audio data for a user of the audio processing module.
struct AudioDataState {
  AudioDataState(const std::string& capture_file_name,
                 int capture_rate_hz,
                 int capture_channels,
                 const std::string& render_file_name,
                 int render_rate_hz,
                 int render_channels)
      : capture_audio(capture_file_name, capture_rate_hz, capture_rate_hz),
        capture_file_channels(capture_channels),
        render_audio(render_file_name, render_rate_hz, render_rate_hz),
        render_file_channels(render_channels),
        capture_config(capture_rate_hz, capture_channels),
        render_config(render_rate_hz, render_channels),
        output_config(capture_rate_hz, capture_channels),
        capture(capture_config.num_frames(), capture_config.num_channels()),
        render(render_config.num_frames(), render_config.num_channels()),
        output(output_config.num_frames(), output_config.num_channels()) {}

  ResampleInputAudioFile capture_audio;
  const int capture_file_channels;
  ResampleInputAudioFile render_audio;
  const int render_file_channels;

  StreamConfig capture_config;
  StreamConfig render_config;
  StreamConfig output_config;

  ChannelBuffer<float> capture;
  ChannelBuffer<float> render;
  ChannelBuffer<float> output;
};

const int kExternalFilterLength = 10;

// Container for external filter parameter.
struct ExternalFilterParameters {
  ExternalFilterParameters() { coefficients.fill(0.f); }
  std::array<float, kExternalFilterLength> coefficients;
  int delay_in_samples = 0;
};

// Message pipe for passing filter parameters
class ExternalFilterMessagePipe : public rtc::RefCountInterface {
 public:
  ExternalFilterMessagePipe();

  bool Post(ExternalFilterParameters* new_filter) {
    bool posting_succeeded = queue_.Insert(new_filter);
    return posting_succeeded;
  }

  bool Receive(ExternalFilterParameters* new_filter) {
    bool new_filter_received = queue_.Remove(new_filter);
    return new_filter_received;
  }

 private:
  SwapQueue<ExternalFilterParameters> queue_;
};

// Audio enhancer implementation performing filtering using externally supplied
// filter parameters.
class ExternalFilterApplier : public AudioEnhancer {
 public:
  ExternalFilterApplier(int sample_rate_hz,
                        size_t num_input_channels,
                        const ExternalFilterParameters& filter);

  void SetFilter(const std::array<float, kExternalFilterLength>& new_filter);

  void Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
               const std::vector<std::array<float, 65>*>& X0_fft_im,
               std::vector<std::vector<std::vector<float>>>* x,
               std::array<float, 65>* denoising_gains,
               float* high_bands_denoising_gain,
               std::array<float, 65>* level_adjustment_gains,
               float* high_bands_level_adjustment_gain) override;

  float AlgorithmicDelayInMs() const override { return delay_in_ms_; }
  bool ModifiesInputSignal() const override { return true; }
  size_t NumOutputChannels() const override { return num_output_channels_; }

 private:
  const int sample_rate_hz_;
  const size_t num_output_channels_;
  const float delay_in_ms_;
  std::array<float, kExternalFilterLength> low_band_filter_;
  std::vector<float> old_data_;
};

// Controller implementation for creating ExternalFilterApplierFactory
// ExternalFilterApplier enhancers and for passing external filter parameter
// data to the enhancers.
class ExternalFilterApplierFactory : public AudioEnhancerController {
 public:
  ExternalFilterApplierFactory(
      rtc::scoped_refptr<ExternalFilterMessagePipe> message_pipe,
      const ExternalFilterParameters& default_filter);
  rtc::scoped_refptr<AudioEnhancer> Create(int sample_rate_hz,
                                           int num_input_channels) override;

  void UpdateEnhancementProperties() override;

  ~ExternalFilterApplierFactory() override {}

 private:
  rtc::scoped_refptr<ExternalFilterMessagePipe> message_pipe_;
  rtc::scoped_refptr<ExternalFilterApplier> current_enhancer_;
  ExternalFilterParameters current_filter_;
};

// Class for computing new parameters and passing those to the audio enhancer.
class ExternalFilterComputer {
 public:
  explicit ExternalFilterComputer(
      rtc::scoped_refptr<ExternalFilterMessagePipe> message_pipe);

  bool SendNewFilter(bool use_zero_filter);

  ExternalFilterParameters CreateFilter(bool use_zero_filter);

 private:
  rtc::scoped_refptr<ExternalFilterMessagePipe> message_pipe_;
  std::array<float, kExternalFilterLength> non_zero_filter_;
  int non_zero_filter_delay_samples_;
  std::array<float, kExternalFilterLength> zero_filter_;
};

ExternalFilterMessagePipe::ExternalFilterMessagePipe()
    : queue_(1, ExternalFilterParameters()) {}

ExternalFilterApplier::ExternalFilterApplier(
    int sample_rate_hz,
    size_t num_input_channels,
    const ExternalFilterParameters& filter)
    : sample_rate_hz_(sample_rate_hz),
      num_output_channels_(num_input_channels),
      delay_in_ms_(filter.delay_in_samples * 1000.f / sample_rate_hz),
      low_band_filter_(filter.coefficients) {}

void ExternalFilterApplier::Process(
    const std::vector<std::array<float, 65>*>& X0_fft_re,
    const std::vector<std::array<float, 65>*>& X0_fft_im,
    std::vector<std::vector<std::vector<float>>>* x,
    std::array<float, 65>* denoising_gains,
    float* high_bands_denoising_gain,
    std::array<float, 65>* level_adjustment_gains,
    float* high_bands_level_adjustment_gain) {
  denoising_gains->fill(1.f);
  level_adjustment_gains->fill(1.f);
  *high_bands_denoising_gain = 1.f;
  *high_bands_level_adjustment_gain = 1.f;

  static_cast<void>(sample_rate_hz_);

  // Code for delaying the upper bands by a delay that matches sample_rate_hz.

  // Code for filtering the lower band using the supplied filter.
}

void ExternalFilterApplier::SetFilter(
    const std::array<float, kExternalFilterLength>& new_filter) {
  std::copy(new_filter.begin(), new_filter.end(), low_band_filter_.begin());
}

ExternalFilterApplierFactory::ExternalFilterApplierFactory(
    rtc::scoped_refptr<ExternalFilterMessagePipe> message_pipe,
    const ExternalFilterParameters& default_filter)
    : message_pipe_(message_pipe), current_filter_(default_filter) {}

void ExternalFilterApplierFactory::UpdateEnhancementProperties() {
  bool new_filter_received = message_pipe_->Receive(&current_filter_);

  if (new_filter_received && current_enhancer_) {
    current_enhancer_->SetFilter(current_filter_.coefficients);
  }
}

rtc::scoped_refptr<AudioEnhancer> ExternalFilterApplierFactory::Create(
    int sample_rate_hz,
    int num_input_channels) {
  current_enhancer_ = new rtc::RefCountedObject<ExternalFilterApplier>(
      sample_rate_hz, num_input_channels, current_filter_);
  return current_enhancer_;
}

ExternalFilterComputer::ExternalFilterComputer(
    rtc::scoped_refptr<ExternalFilterMessagePipe> message_pipe)
    : message_pipe_(message_pipe) {
  non_zero_filter_.fill(0.f);
  non_zero_filter_[4] = .5f;
  non_zero_filter_delay_samples_ = 4;
  zero_filter_.fill(0.f);
}

bool ExternalFilterComputer::SendNewFilter(bool use_zero_filter) {
  ExternalFilterParameters new_filter = CreateFilter(use_zero_filter);
  bool successful_post = message_pipe_->Post(&new_filter);
  return successful_post;
}

ExternalFilterParameters ExternalFilterComputer::CreateFilter(
    bool use_zero_filter) {
  ExternalFilterParameters new_filter;
  new_filter.delay_in_samples =
      use_zero_filter ? 0 : non_zero_filter_delay_samples_;
  const auto& new_filter_coefficients =
      use_zero_filter ? zero_filter_ : non_zero_filter_;
  std::copy(new_filter_coefficients.begin(), new_filter_coefficients.end(),
            new_filter.coefficients.begin());

  return ExternalFilterParameters();
}

// Provides an AudioEnhancer implementation of a multi-channel downmixing using
// a microphone selection.
class MicSelector : public AudioEnhancer {
 public:
  explicit MicSelector(size_t num_input_channels);

  void Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
               const std::vector<std::array<float, 65>*>& X0_fft_im,
               std::vector<std::vector<std::vector<float>>>* x,
               std::array<float, 65>* denoising_gains,
               float* high_bands_denoising_gain,
               std::array<float, 65>* level_adjustment_gains,
               float* high_bands_level_adjustment_gain) override;

  float AlgorithmicDelayInMs() const override { return 0.f; }
  bool ModifiesInputSignal() const override { return true; }
  size_t NumOutputChannels() const override { return 1; }

 private:
  std::vector<float> average_mic_powers_;
  int selected_channel_ = -1;
  int prev_strongest_channel_ = -1;
  size_t num_blocks_with_same_selection_ = 0;
};

// Factoru for producing MicSelector enhancers.
class MicSelectorFactory : public AudioEnhancerController {
 public:
  MicSelectorFactory();
  rtc::scoped_refptr<AudioEnhancer> Create(int sample_rate_hz,
                                           int num_input_channels) override;

  void UpdateEnhancementProperties() override {}

  ~MicSelectorFactory() override {}

 private:
};

MicSelector::MicSelector(size_t num_input_channels)
    : average_mic_powers_(num_input_channels, 0.f) {}

void MicSelector::Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
                          const std::vector<std::array<float, 65>*>& X0_fft_im,
                          std::vector<std::vector<std::vector<float>>>* x,
                          std::array<float, 65>* denoising_gains,
                          float* high_bands_denoising_gain,
                          std::array<float, 65>* level_adjustment_gains,
                          float* high_bands_level_adjustment_gain) {
  for (size_t ch = 0; ch < (*x)[0].size(); ++ch) {
    float power = 0.f;
    for (size_t k = 0; k < (*x)[0][ch].size(); ++k) {
      power += (*x)[0][ch][k] * (*x)[0][ch][k];
    }
    average_mic_powers_[ch] += 0.05f * (power - average_mic_powers_[ch]);
  }

  int strongest_ch = 0;
  for (size_t ch = 1; ch < (*x)[0].size(); ++ch) {
    if (average_mic_powers_[ch] > average_mic_powers_[strongest_ch]) {
      strongest_ch = ch;
    }
  }

  num_blocks_with_same_selection_ = strongest_ch == prev_strongest_channel_
                                        ? num_blocks_with_same_selection_ + 1
                                        : 0;

  prev_strongest_channel_ = strongest_ch;

  if (num_blocks_with_same_selection_ > 100) {
    selected_channel_ = strongest_ch;
  }

  if (selected_channel_ == -1) {
    float one_by_num_channels = 1.f / (*x)[0].size();
    for (size_t block = 0; block < x->size(); ++block) {
      for (size_t ch = 1; ch < (*x)[block].size(); ++ch) {
        for (size_t k = 0; k < (*x)[block][ch].size(); ++k) {
          (*x)[block][0][k] += (*x)[block][ch][k];
        }
      }
      for (size_t k = 0; k < (*x)[block][0].size(); ++k) {
        (*x)[block][0][k] *= one_by_num_channels;
      }
    }
  } else if (selected_channel_ != 0) {
    for (size_t block = 0; block < x->size(); ++block) {
      std::copy((*x)[block][selected_channel_].begin(),
                (*x)[block][selected_channel_].end(), (*x)[block][0].begin());
    }
  }
}

MicSelectorFactory::MicSelectorFactory() {}

rtc::scoped_refptr<AudioEnhancer> MicSelectorFactory::Create(
    int sample_rate_hz,
    int num_input_channels) {
  return new rtc::RefCountedObject<MicSelector>(num_input_channels);
}

// Provides AudioEnhancer functionality that applies random denoising and
// amplification gains.
class RandomGainGenerator : public AudioEnhancer {
 public:
  explicit RandomGainGenerator(size_t num_input_channels);

  void Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
               const std::vector<std::array<float, 65>*>& X0_fft_im,
               std::vector<std::vector<std::vector<float>>>* x,
               std::array<float, 65>* denoising_gains,
               float* high_bands_denoising_gain,
               std::array<float, 65>* level_adjustment_gains,
               float* high_bands_level_adjustment_gain) override;

  float AlgorithmicDelayInMs() const override { return 10.f; }
  bool ModifiesInputSignal() const override { return false; }
  size_t NumOutputChannels() const override { return num_output_channels_; }

 private:
  const size_t num_output_channels_;
  Random rand_gen_;
};

// Factory for producing RandomGainGenerators.
class RandomGainGeneratorFactory : public AudioEnhancerController {
 public:
  RandomGainGeneratorFactory();
  rtc::scoped_refptr<AudioEnhancer> Create(int sample_rate_hz,
                                           int num_input_channels) override;

  void UpdateEnhancementProperties() override {}

  ~RandomGainGeneratorFactory() override {}

 private:
};

// Produces a random attenuating gain between 0 and 1.
float GetRandomAttenuatingGain(Random* rand_gen) {
  constexpr int32_t apmlitude = 10000;
  return rand_gen->Rand(0, apmlitude) / static_cast<float>(apmlitude);
}

// Produces a random amplification gain between 0.1 and 10.1.
float GetRandomAmplificationGain(Random* rand_gen) {
  constexpr int32_t apmlitude = 10000;
  return rand_gen->Rand(0, apmlitude) / static_cast<float>(apmlitude) * 10.f +
         0.1f;
}

RandomGainGenerator::RandomGainGenerator(size_t num_input_channels)
    : num_output_channels_(num_input_channels), rand_gen_(42) {}

void RandomGainGenerator::Process(
    const std::vector<std::array<float, 65>*>& X0_fft_re,
    const std::vector<std::array<float, 65>*>& X0_fft_im,
    std::vector<std::vector<std::vector<float>>>* x,
    std::array<float, 65>* denoising_gains,
    float* high_bands_denoising_gain,
    std::array<float, 65>* level_adjustment_gains,
    float* high_bands_level_adjustment_gain) {
  for (size_t k = 0; k < denoising_gains->size(); ++k) {
    (*denoising_gains)[k] = GetRandomAttenuatingGain(&rand_gen_);
  }
  *high_bands_denoising_gain = GetRandomAttenuatingGain(&rand_gen_);

  for (size_t k = 0; k < level_adjustment_gains->size(); ++k) {
    (*level_adjustment_gains)[k] = GetRandomAmplificationGain(&rand_gen_);
  }
  *high_bands_level_adjustment_gain = GetRandomAmplificationGain(&rand_gen_);
}

RandomGainGeneratorFactory::RandomGainGeneratorFactory() {}

rtc::scoped_refptr<AudioEnhancer> RandomGainGeneratorFactory::Create(
    int sample_rate_hz,
    int num_input_channels) {
  return new rtc::RefCountedObject<RandomGainGenerator>(num_input_channels);
}

}  // namespace

TEST(AudioEnhancerTest, PassingExternalFilterParameters) {
  // Set up audio data and buffers.
  AudioDataState ads(ResourcePath("near32_stereo", "pcm"), 32000, 2,
                     ResourcePath("far32_stereo", "pcm"), 32000, 2);

  // APM configuration
  Config config;
  AudioProcessing::Config apm_config;
  apm_config.echo_canceller.enabled = true;

  // Create APM builder.
  AudioProcessingBuilder ap_builder;

  // Create objects for the audio enhancement and add to the builder.
  rtc::scoped_refptr<ExternalFilterMessagePipe> message_pipe =
      new rtc::RefCountedObject<ExternalFilterMessagePipe>();
  std::unique_ptr<ExternalFilterComputer> external_filter_computer_ =
      std::make_unique<ExternalFilterComputer>(message_pipe);
  ExternalFilterParameters default_filter =
      external_filter_computer_->CreateFilter(false);
  std::unique_ptr<AudioEnhancerController> audio_enhancer_controller =
      std::make_unique<ExternalFilterApplierFactory>(message_pipe,
                                                     default_filter);
  ap_builder.SetAudioEnhancerController(std::move(audio_enhancer_controller));

  // Optionally create the AEC3 factory and add to the builder
  EchoCanceller3Config cfg;
  std::unique_ptr<EchoControlFactory> echo_control_factory =
      std::make_unique<EchoCanceller3Factory>(cfg);
  ap_builder.SetEchoControlFactory(std::move(echo_control_factory));

  // Create APM.
  std::unique_ptr<AudioProcessing> ap;
  ap.reset(ap_builder.Create(config));
  ap->ApplyConfig(apm_config);

  // Apply processing.
  for (size_t i = 0; i < 100; ++i) {
    // Read audio data.
    ReadAudioFrame(&ads.render_audio, ads.render_config, ads.render.channels());
    ReadAudioFrame(&ads.capture_audio, ads.capture_config,
                   ads.capture.channels());

    // Let the filter computer send a new filter to the enhancer every 10th
    // frame.
    if (i % 10 == 0) {
      external_filter_computer_->SendNewFilter(i % 20 == 0);
    }

    // Set side-information required by APM.
    ap->set_stream_delay_ms(100);
    ap->set_stream_analog_level(100);

    // Call the APM Processing APIs.
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap->ProcessStream(ads.capture.channels(), ads.capture_config,
                                   ads.output_config, ads.output.channels()));

    RTC_CHECK_EQ(
        AudioProcessing::kNoError,
        ap->ProcessReverseStream(ads.render.channels(), ads.render_config,
                                 ads.render_config, ads.render.channels()));
  }
}

TEST(AudioEnhancerTest, DownmixingToFewerChannels) {
  // Set up audio data and buffers.
  AudioDataState ads(ResourcePath("near32_stereo", "pcm"), 32000, 2,
                     ResourcePath("far32_stereo", "pcm"), 32000, 2);

  // APM configuration
  Config config;
  AudioProcessing::Config apm_config;
  apm_config.echo_canceller.enabled = true;

  // Create APM builder.
  AudioProcessingBuilder ap_builder;

  // Create objects for the audio enhancement and add to the builder.
  std::unique_ptr<AudioEnhancerController> audio_enhancer_controller =
      std::make_unique<MicSelectorFactory>();
  ap_builder.SetAudioEnhancerController(std::move(audio_enhancer_controller));

  // Optionally create the AEC3 factory and add to the builder
  EchoCanceller3Config cfg;
  std::unique_ptr<EchoControlFactory> echo_control_factory =
      std::make_unique<EchoCanceller3Factory>(cfg);
  ap_builder.SetEchoControlFactory(std::move(echo_control_factory));

  // Create APM.
  std::unique_ptr<AudioProcessing> ap;
  ap.reset(ap_builder.Create(config));
  ap->ApplyConfig(apm_config);

  // Apply processing.
  for (size_t i = 0; i < 100; ++i) {
    // Read audio data.
    ReadAudioFrame(&ads.render_audio, ads.render_config, ads.render.channels());
    ReadAudioFrame(&ads.capture_audio, ads.capture_config,
                   ads.capture.channels());

    // Set side-information required by APM.
    ap->set_stream_delay_ms(100);
    ap->set_stream_analog_level(100);

    // Call the APM Processing APIs.
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap->ProcessStream(ads.capture.channels(), ads.capture_config,
                                   ads.output_config, ads.output.channels()));

    RTC_CHECK_EQ(
        AudioProcessing::kNoError,
        ap->ProcessReverseStream(ads.render.channels(), ads.render_config,
                                 ads.render_config, ads.render.channels()));
  }
}

TEST(AudioEnhancerTest, PerformEnhancementViaGainProduction) {
  // Set up audio data and buffers.
  AudioDataState ads(ResourcePath("near32_stereo", "pcm"), 32000, 2,
                     ResourcePath("far32_stereo", "pcm"), 32000, 2);

  // APM configuration
  Config config;
  AudioProcessing::Config apm_config;
  apm_config.echo_canceller.enabled = true;

  // Create APM builder.
  AudioProcessingBuilder ap_builder;

  // Create objects for the audio enhancement and add to the builder.
  std::unique_ptr<AudioEnhancerController> audio_enhancer_controller =
      std::make_unique<RandomGainGeneratorFactory>();
  ap_builder.SetAudioEnhancerController(std::move(audio_enhancer_controller));

  // Optionally create the AEC3 factory and add to the builder
  EchoCanceller3Config cfg;
  std::unique_ptr<EchoControlFactory> echo_control_factory =
      std::make_unique<EchoCanceller3Factory>(cfg);
  ap_builder.SetEchoControlFactory(std::move(echo_control_factory));

  // Create APM.
  std::unique_ptr<AudioProcessing> ap;
  ap.reset(ap_builder.Create(config));
  ap->ApplyConfig(apm_config);

  // Apply processing.
  for (size_t i = 0; i < 100; ++i) {
    // Read audio data.
    ReadAudioFrame(&ads.render_audio, ads.render_config, ads.render.channels());
    ReadAudioFrame(&ads.capture_audio, ads.capture_config,
                   ads.capture.channels());

    // Set side-information required by APM.
    ap->set_stream_delay_ms(100);
    ap->set_stream_analog_level(100);

    // Call the APM Processing APIs.
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap->ProcessStream(ads.capture.channels(), ads.capture_config,
                                   ads.output_config, ads.output.channels()));

    RTC_CHECK_EQ(
        AudioProcessing::kNoError,
        ap->ProcessReverseStream(ads.render.channels(), ads.render_config,
                                 ads.render_config, ads.render.channels()));
  }
}

}  // namespace test
}  // namespace webrtc
