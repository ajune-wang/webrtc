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

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

// TODO(alessiob): Remove anon NS if empty.

}  // namespace

// Check that the RNN VAD features difference between (i) those computed
// using the porting and (ii) those computed using the reference code is within
// a tolerance.
TEST(RnnVadTest, CheckExtractedFeaturesAreNear) {
  // TODO(alessiob): Implement.
}

}  // namespace test
}  // namespace webrtc
