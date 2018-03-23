/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/downsample.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {

void Decimate48k24k(rtc::ArrayView<const float> src,
                    rtc::ArrayView<float> dst) {
  RTC_DCHECK_EQ(2 * dst.size(), src.size());
  // TODO(alessiob): Use a better anti-aliasing filter.
  dst[0] = 0.5 * src[0] + 0.25 * src[1];
  for (size_t i = 1; i < dst.size(); ++i)
    dst[i] = 0.25 * src[2 * i - 1] + 0.5 * src[2 * i] + 0.25 * src[2 * i + 1];
}

void Decimate2xNoAntiAliasignFilter(rtc::ArrayView<const float> src,
                                    rtc::ArrayView<float> dst) {
  RTC_DCHECK_EQ(2 * dst.size(), src.size());
  for (size_t i = 0; i < dst.size(); ++i)
    dst[i] = src[2 * i];
}

}  // namespace rnn_vad
}  // namespace webrtc
