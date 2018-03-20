/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <fstream>

#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/downsample.h"
#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/rnn_vad.h"
#include "common_audio/wav_file.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {

constexpr size_t kInputSampleRate = 48000;
constexpr size_t kInputAudioFrameSize = 480;

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
  if (wav_reader.sample_rate() != kInputSampleRate) {
    RTC_LOG(LS_ERROR) << "The sample rate rate must be " << kInputSampleRate
                      << " (" << wav_reader.sample_rate() << " found)";
    return 1;
  }

  // Init.
  std::ofstream out_vad_probs(FLAG_o, std::ofstream::binary);
  // Frame samples.
  std::array<float, kInputAudioFrameSize> samples;
  std::array<float, rnn_vad::k10msFrameSize> samples_decimated;
  // Feature extractor and VAD.
  rnn_vad::RnnVadFeaturesExtractor features_extractor;
  rnn_vad::RnnBasedVad vad;

  // Compute VAD probabilities.
  while (true) {
    // Read frame at the input sample rate.
    const auto read_samples =
        wav_reader.ReadSamples(kInputAudioFrameSize, samples.data());
    if (read_samples < kInputAudioFrameSize)
      break;  // EOF.
    // Downsample.
    rnn_vad::Decimate48k24k(
        {samples.data(), samples.size()},
        {samples_decimated.data(), samples_decimated.size()});

    float vad_probability;
    // Extract features and provide them to the VAD.
    features_extractor.ComputeFeatures(
        {samples_decimated.data(), samples_decimated.size()});
    if (features_extractor.is_silence()) {
      vad_probability = 0.f;
      // TODO(alessiob): Reset RNN state.
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
