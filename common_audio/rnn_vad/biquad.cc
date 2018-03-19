/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/biquad.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {

BiQuadFilter::BiQuadFilter() = default;

BiQuadFilter::BiQuadFilter(const Config config) : config_(config) {}

BiQuadFilter::~BiQuadFilter() = default;

void BiQuadFilter::SetState(const State state) {
  state_ = state;
}

// Transposed direct form II implementation of a bi-quad filter.
void BiQuadFilter::ProcessFrame(rtc::ArrayView<const float> x,
                                rtc::ArrayView<float> y) {
  RTC_DCHECK_EQ(x.size(), y.size());
  for (size_t i = 0; i < y.size(); ++i) {
    float x_i = x[i];
    float y_i = config_.b0 * x[i] + state_.m0;
    state_.m0 = state_.m1 + (config_.b1 * static_cast<double>(x_i) -
                             config_.a1 * static_cast<double>(y_i));
    state_.m1 = (config_.b2 * static_cast<double>(x_i) -
                 config_.a2 * static_cast<double>(y_i));
    y[i] = y_i;
  }
}

}  // namespace rnn_vad
}  // namespace webrtc
