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

#if RTC_HAS_MSAN
#include <sanitizer/msan_interface.h>
#endif

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
  Bar bar = MsanMarkUninitializedCopy<Bar>({});
  EXPECT_EQ(0u, bar.ID);
  EXPECT_EQ(0u, bar.foo.field1);
  EXPECT_EQ(0u, bar.foo.field2);

#if RTC_HAS_MSAN
  __msan_set_expect_umr(1);
#endif
  Bar mini_bar;  // Without MsanMarkUninitializedCopy.
  if (mini_bar.ID > 50)
    mini_bar.foo.field1++;
#if RTC_HAS_MSAN
  __msan_set_expect_umr(0);
#endif
}

}  // namespace rtc
