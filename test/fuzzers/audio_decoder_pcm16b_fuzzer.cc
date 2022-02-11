/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/pcm16b/audio_decoder_pcm16b.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 10000 || size < 2) {
    return;
  }

  int sample_rate_hz;
  switch (data[0] % 4) {
    case 0:
      sample_rate_hz = 8000;
      break;
    case 1:
      sample_rate_hz = 16000;
      break;
    case 2:
      sample_rate_hz = 32000;
      break;
    case 3:
      sample_rate_hz = 48000;
      break;
    default:
      RTC_DCHECK_NOTREACHED();
      return;
  }
  const size_t num_channels = data[1] % 16 + 1;
  RTC_DCHECK_GE(num_channels, 1);
  RTC_DCHECK_LE(num_channels, 16);

  // Two first bytes of the data are used. Move forward.
  data += 2;
  size -= 2;

  AudioDecoderPcm16B dec(sample_rate_hz, num_channels);
  // Allocate a maximum output size of 100 ms.
  static const size_t kAllocatedOuputSizeSamples =
      sample_rate_hz * num_channels / 10;
  int16_t output[kAllocatedOuputSizeSamples];
  FuzzAudioDecoder(DecoderFunctionType::kNormalDecode, data, size, &dec,
                   sample_rate_hz, sizeof(output), output);
}
}  // namespace webrtc
