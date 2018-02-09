/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_RNN_VAD_H_
#define COMMON_AUDIO_RNN_VAD_RNN_VAD_H_

#include "common_audio/rnn_vad/rnn_data.pb.h"
// TODO(alessiob): Once rnn_data.pb.h can be included, use it in RnnVad.

namespace webrtc {

class RnnVad {
 public:
  RnnVad() : flag(false) {}
  ~RnnVad() = default;
  void Toggle() { flag = !flag; }
 private:
  bool flag;
};

}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_RNN_VAD_H_
