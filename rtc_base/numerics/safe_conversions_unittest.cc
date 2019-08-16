/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/safe_conversions.h"

#include <limits>

#include "test/gtest.h"

namespace rtc {

TEST(SaturatedCast, Int64ToDoubleWithinRangeCastCausesOverflow) {
  EXPECT_EQ(rtc::saturated_cast<int64_t>(9223372036854775800.0),
            std::numeric_limits<int64_t>::max());
}

}  // namespace rtc
