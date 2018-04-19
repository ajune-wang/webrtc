/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_COMMON_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_COMMON_H_

namespace webrtc {
namespace rnn_vad {

constexpr size_t kSampleRate24kHz = 24000;
constexpr size_t kFrameSize10ms24kHz = kSampleRate24kHz / 100;
constexpr size_t kFrameSize20ms24kHz = kFrameSize10ms24kHz * 2;

// Pitch analysis params.
constexpr size_t kPitchMinPeriod24kHz = kSampleRate24kHz / 800;   // 0.00125 s.
constexpr size_t kPitchMaxPeriod24kHz = kSampleRate24kHz / 62.5;  // 0.016 s.
constexpr size_t kBufSize24kHz = kPitchMaxPeriod24kHz + kFrameSize20ms24kHz;
static_assert((kBufSize24kHz & 1) == 0, "The buffer size must be even.");

// Define a higher minimum pitch period for the initial search. This is used to
// avoid searching for very short periods, for which a refinement step is
// responsible.
constexpr size_t kPitchMinPeriod24kHzPitchSearch = 3 * kPitchMinPeriod24kHz;
static_assert(kPitchMinPeriod24kHz < kPitchMinPeriod24kHzPitchSearch, "");
static_assert(kPitchMinPeriod24kHzPitchSearch < kPitchMaxPeriod24kHz, "");

// 12 kHz analysis.
constexpr size_t kSampleRate12kHz = 12000;
constexpr size_t kFrameSize10ms12kHz = kSampleRate12kHz / 100;
constexpr size_t kFrameSize20ms12kHz = kFrameSize10ms12kHz * 2;
constexpr size_t kBufSize12kHz = kBufSize24kHz / 2;
constexpr size_t kPitchMinPeriod12kHzPitchSearch =
    kPitchMinPeriod24kHzPitchSearch / 2;
constexpr size_t kPitchMaxPeriod12kHz = kPitchMaxPeriod24kHz / 2;

// 48 kHz constants.
constexpr size_t kPitchMinPeriod48kHz = kPitchMinPeriod24kHz * 2;
constexpr size_t kPitchMaxPeriod48kHz = kPitchMaxPeriod24kHz * 2;

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_COMMON_H_
