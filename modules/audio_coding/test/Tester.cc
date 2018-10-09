/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string>
#include <vector>

#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/test/TestRedFec.h"
#include "modules/audio_coding/test/TestVADDTX.h"
#include "modules/audio_coding/test/TwoWayCommunication.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

// This parameter is used to describe how to run the tests. It is normally
// set to 0, and all tests are run in quite mode.
#define ACM_TEST_MODE 0

TEST(AudioCodingModuleTest, TestRedFec) {
  webrtc::TestRedFec().Perform();
}

#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)) && \
    defined(WEBRTC_CODEC_ILBC)
#if defined(WEBRTC_ANDROID)
TEST(AudioCodingModuleTest, DISABLED_TwoWayCommunication) {
#else
TEST(AudioCodingModuleTest, TwoWayCommunication) {
#endif
  webrtc::TwoWayCommunication().Perform();
}
#endif

TEST(AudioCodingModuleTest, TestWebRtcVadDtx) {
  webrtc::TestWebRtcVadDtx().Perform();
}

TEST(AudioCodingModuleTest, TestOpusDtx) {
  webrtc::TestOpusDtx().Perform();
}

// The full API test is too long to run automatically on bots, but can be used
// for offline testing. User interaction is needed.
#ifdef ACM_TEST_FULL_API
TEST(AudioCodingModuleTest, TestAPI) {
  webrtc::APITest().Perform();
}
#endif
