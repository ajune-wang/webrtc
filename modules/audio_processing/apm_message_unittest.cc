/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/apm_message.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(ApmMessageTest, TestAllPayloadTypes) {
  {
    ApmMessage m = {ApmMessage::TEST, .int_val = 100};
    EXPECT_EQ(m.id, ApmMessage::TEST);
    EXPECT_EQ(m.int_val, 100);
  }

  {
    ApmMessage m = {ApmMessage::TEST, .float_val = 100};
    EXPECT_EQ(m.id, ApmMessage::TEST);
    EXPECT_EQ(m.float_val, 100.f);
  }
}

}  // namespace test
}  // namespace webrtc
