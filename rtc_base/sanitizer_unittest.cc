/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/sanitizer.h"
#include "test/gtest.h"

namespace rtc {
namespace {

struct Foo {
  uint32_t field1;
  uint16_t field2;
};

struct Bar {
  uint32_t ID;
  Foo foo;
};

}  // namespace

TEST(SanitizerTest, MsanZeroedUninitialized) {
  Bar bar = MsanZeroedUninitialized<Bar>();
  EXPECT_EQ(0u, bar.ID);
  EXPECT_EQ(0u, bar.foo.field1);
  EXPECT_EQ(0u, bar.foo.field2);

  // The code below should be caught by memsan.
  Bar mini_bar;
  if (mini_bar.ID > 50)
    mini_bar.foo.field1++;
}

}  // namespace rtc
