/*
 Copyright (c) 2020 The WebRTC Project Authors. All rights reserved.

 Use of this source code is governed by a BSD-style license
 that can be found in the LICENSE file in the root of the source
 tree. An additional intellectual property rights grant can be found
 in the file PATENTS.  All contributing project authors may
 be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <winsock2.h>

#include "common_audio/vad/include/webrtc_vad.h"
#include "common_audio/wav_file.h"

int main(int argc, char* argv[]) {
  constexpr auto NUM_SAMPLES = 30 * 16000 / 1000;

  VadInst* vad;

  int16_t pcm_samples[NUM_SAMPLES] = {0};
  size_t pcm_read;
  int is_voice;

  webrtc::WavReader reader(argv[1]);

  webrtc::WavWriter writer(argv[2], 16000, 1);

  vad = WebRtcVad_Create();

  WebRtcVad_Init(vad);

  WebRtcVad_set_mode(vad, 3);

  while ((pcm_read = reader.ReadSamples(NUM_SAMPLES, pcm_samples)) > 0) {
    if (pcm_read < NUM_SAMPLES) {
      memset(&pcm_samples[pcm_read], 0,
             (NUM_SAMPLES - pcm_read) * sizeof(*pcm_samples));
    }
    is_voice = WebRtcVad_Process(vad, 16000, pcm_samples, NUM_SAMPLES);
    if (is_voice == 1) {
      writer.WriteSamples(pcm_samples, pcm_read);
    }
    printf("%d\n", is_voice);
  }

  WebRtcVad_Free(vad);

  return 0;
}
