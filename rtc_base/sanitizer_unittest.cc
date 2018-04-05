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

// TODO(b/8762): Remove when std::is_trivially_copyable becomes available in
// downstream projects.
#include <vector>

#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"

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

// TODO(b/8762): Remove Baz when std::is_trivially_copyable becomes available in
// downstream projects.
struct Baz {  // Not trivially copyable.
  std::vector<int> v;
};

// Run the callback, and crash if it *doesn't* make an uninitialized memory
// read. If MSan isn't on, just run the callback.
template <typename F>
void MsanExpectUninitializedRead(F&& f) {
#if RTC_HAS_MSAN
  // Allow uninitialized memory reads.
  RTC_LOG(LS_INFO) << "__msan_set_expect_umr(1)";
  __msan_set_expect_umr(1);
#endif
  f();
#if RTC_HAS_MSAN
  // Disallow uninitialized memory reads again, and verify that at least
  // one uninitialized memory read happened while we weren't looking.
  RTC_LOG(LS_INFO) << "__msan_set_expect_umr(0)";
  __msan_set_expect_umr(0);
#endif
}

}  // namespace

// TODO(b/8762): Remove the test below when std::is_trivially_copyable becomes
// available in downstream projects.
TEST(SanitizerTest, Preconditions) {
  static_assert(IsTriviallyCopyable<Foo>(), "");
  static_assert(IsTriviallyCopyable<Bar>(), "");
  static_assert(!IsTriviallyCopyable<Baz>(), "");
}

TEST(SanitizerTest, MsanUninitialized) {
  Bar bar = MsanUninitialized<Bar>({});
  // Check that a read after initialization is OK.
  bar.ID = 1;
  EXPECT_EQ(1u, bar.ID);
  RTC_LOG(LS_INFO) << "read after init passed";
  // Check that other fields are uninitialized and equal to zero.
  MsanExpectUninitializedRead([&] { EXPECT_EQ(0u, bar.foo.field1); });
  MsanExpectUninitializedRead([&] { EXPECT_EQ(0u, bar.foo.field2); });
  RTC_LOG(LS_INFO) << "read with no init passed";
}

}  // namespace rtc
