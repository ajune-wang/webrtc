/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/rnn_vad.h"

#include <fstream>

#include "api/array_view.h"
#include "common_audio/wav_file.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kSampleRate = 48000;

const char kUsageDescription[] =
    "Usage: rnn_vad_tool\n"
    "          -i </path/to/source/audiotrack.wav>\n"
    "          -o </path/to/output.dat>\n"
    "\n\n"
    "VAD based on a light-weight RNN-based classifier.\n";

DEFINE_string(i, "", "Path to the input wav file");
DEFINE_string(o, "", "Path to the output file");
DEFINE_bool(help, false, "Prints this message");

}  // namespace

int main(int argc, char* argv[]) {
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) || FLAG_help ||
      argc != 1) {
    printf("%s", kUsageDescription);
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }

  // Open wav input file and check properties.
  WavReader wav_reader(FLAG_i);
  if (wav_reader.num_channels() != 1) {
    RTC_LOG(LS_ERROR) << "Only mono wav files are supported";
    return 1;
  }
  if (wav_reader.sample_rate() != kSampleRate) {
    RTC_LOG(LS_ERROR) << "The sample rate rate must be " << kSampleRate << " ("
                      << wav_reader.sample_rate() << " found)";
    return 1;
  }

  // Init.
  std::ofstream out_vad_probs(FLAG_o, std::ofstream::binary);
  // Frame samples.
  std::array<int16_t, rnn_vad::kAudioFrameSize> samples;
  rtc::ArrayView<const int16_t, rnn_vad::kAudioFrameSize> samples_view(
      samples.data(), rnn_vad::kAudioFrameSize);
  // Feature extractor and VAD.
  RnnVadFeaturesExtractor features_extractor;
  RnnBasedVad vad;

  // Compute VAD probabilities.
  while (true) {
    // Process frame.
    const auto read_samples =
        wav_reader.ReadSamples(rnn_vad::kAudioFrameSize, samples.data());
    if (read_samples < rnn_vad::kAudioFrameSize)
      break;  // EOF.
    float vad_probability;
    // Extract features and provide them to the VAD.
    features_extractor.ComputeFeatures(samples_view);
    if (features_extractor.is_silence()) {
      vad_probability = 0.f;
    } else {
      vad.ComputeVadProbability(features_extractor.GetOutput());
      vad_probability = vad.vad_probability();
    }
    RTC_DCHECK_GE(vad_probability, 0.f);
    RTC_DCHECK_GE(1.f, vad_probability);
    out_vad_probs.write(reinterpret_cast<const char*>(&vad_probability),
                        sizeof(float));
  }
  out_vad_probs.close();

  // TODO(alessiob): Implement.
  return 0;
}

}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
