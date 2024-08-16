/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/payload_type_picker.h"

#include "test/gtest.h"

namespace webrtc {

TEST(PayloadTypePicker, PayloadTypeAssignmentWorks) {
  // Note: This behavior is due to be deprecated and removed.
  PayloadType pt_a(1);
  PayloadType pt_b = 1;  // Implicit conversion
  EXPECT_EQ(pt_a, pt_b);
  int pt_as_int = pt_a;  // Implicit conversion
  EXPECT_EQ(1, pt_as_int);
}

TEST(PayloadTypePicker, InstantiateTypes) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
}

}  // namespace webrtc
