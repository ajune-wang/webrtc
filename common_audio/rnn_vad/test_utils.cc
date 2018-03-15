/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/test_utils.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {

void ExpectNear(rtc::ArrayView<const float> a,
                rtc::ArrayView<const float> b,
                const float tolerance) {
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_NEAR(a[i], b[i], tolerance);
  }
}

}  // namespace test
}  // namespace webrtc
