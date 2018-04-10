/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/resampler/push_sinc_resampler.h"
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/rnn_vad.h"
#include "common_audio/wav_file.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {

constexpr size_t kSampleRate48kHz = 48000;
constexpr size_t kFrameSize10ms48kHz = 480;

DEFINE_string(i, "", "Path to the input wav file");
DEFINE_string(f, "", "Path to the output features file");
DEFINE_string(o, "", "Path to the output VAD probabilities file");
DEFINE_bool(help, false, "Prints this message");

}  // namespace

int main(int argc, char* argv[]) {
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  // Open wav input file and check properties.
  WavReader wav_reader(FLAG_i);
  if (wav_reader.num_channels() != 1) {
    RTC_LOG(LS_ERROR) << "Only mono wav files are supported";
    return 1;
  }
  if (wav_reader.sample_rate() != kSampleRate48kHz) {
    RTC_LOG(LS_ERROR) << "The sample rate rate must be " << kSampleRate48kHz
                      << " (" << wav_reader.sample_rate() << " found)";
    return 1;
  }

  // Init.
  FILE* vad_probs_file = fopen(FLAG_o, "wb");
  FILE* features_file = nullptr;
  if (FLAG_f)
    features_file = fopen(FLAG_f, "wb");
  std::array<float, kFrameSize10ms48kHz> samples_10ms_48kHz;
  std::array<float, rnn_vad::kFrameSize10ms24kHz> samples_10ms_24kHz;
  PushSincResampler decimator(kFrameSize10ms48kHz,
                              rnn_vad::kFrameSize10ms24kHz);
  // Feature extractor and RNN-based VAD.
  rnn_vad::RnnVadFeaturesExtractor features_extractor;
  auto feature_vector_view = features_extractor.GetFeatureVectorView();
  rnn_vad::RnnBasedVad vad;

  // Compute VAD probabilities.
  while (true) {
    // Read frame at the input sample rate.
    const auto read_samples =
        wav_reader.ReadSamples(kFrameSize10ms48kHz, samples_10ms_48kHz.data());
    if (read_samples < kFrameSize10ms48kHz)
      break;  // EOF.
    // Extract features.
    decimator.Resample(samples_10ms_48kHz.data(), samples_10ms_48kHz.size(),
                       samples_10ms_24kHz.data(), samples_10ms_24kHz.size());
    float vad_probability;
    bool is_silence = features_extractor.ComputeFeaturesCheckSilence(
        {samples_10ms_24kHz.data(), samples_10ms_24kHz.size()});
    // Write features.
    if (features_file) {
      const float float_is_silence = is_silence ? 1.f : 0.f;
      fwrite(&float_is_silence, sizeof(float), 1, features_file);
      fwrite(feature_vector_view.data(), sizeof(float),
             feature_vector_view.size(), features_file);
    }
    // Compute VAD probability.
    if (is_silence) {
      vad_probability = 0.f;
      vad.Reset();
    } else {
      vad.ComputeVadProbability(feature_vector_view);
      vad_probability = vad.vad_probability();
    }
    RTC_DCHECK_GE(vad_probability, 0.f);
    RTC_DCHECK_GE(1.f, vad_probability);
    fwrite(&vad_probability, sizeof(float), 1, vad_probs_file);
  }
  // Close output file(s).
  fclose(vad_probs_file);
  RTC_LOG(LS_INFO) << "VAD probabilities written to " << FLAG_o;
  if (features_file) {
    fclose(features_file);
    RTC_LOG(LS_INFO) << "features written to " << FLAG_f;
  }

  return 0;
}

}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
