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

#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"

#if RTC_HAS_MSAN
#include <sanitizer/msan_interface.h>
#endif

namespace rtc {
namespace {

// Test sanitizer_impl::IsTriviallyCopyable (at compile time).

// Trivially copyable.

struct BazTrTrTr {
  BazTrTrTr(const BazTrTrTr&) = default;
  BazTrTrTr& operator=(const BazTrTrTr&) = default;
  ~BazTrTrTr() = default;
};
static_assert(sanitizer_impl::IsTriviallyCopyable<BazTrTrTr>(), "");

struct BazTrDeTr {
  BazTrDeTr(const BazTrDeTr&) = default;
  BazTrDeTr& operator=(const BazTrDeTr&) = delete;
  ~BazTrDeTr() = default;
};
static_assert(sanitizer_impl::IsTriviallyCopyable<BazTrDeTr>(), "");

// Non trivially copyable.

struct BazTrTrNt {
  BazTrTrNt(const BazTrTrNt&) = default;
  BazTrTrNt& operator=(const BazTrTrNt&) = default;
  ~BazTrTrNt();
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazTrTrNt>(), "");

struct BazTrNtTr {
  BazTrNtTr(const BazTrNtTr&) = default;
  BazTrNtTr& operator=(const BazTrNtTr&);
  ~BazTrNtTr() = default;
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazTrNtTr>(), "");

struct BazTrNtNt {
  BazTrNtNt(const BazTrNtNt&) = default;
  BazTrNtNt& operator=(const BazTrNtNt&);
  ~BazTrNtNt();
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazTrNtNt>(), "");

struct BazTrDeNt {
  BazTrDeNt(const BazTrDeNt&) = default;
  BazTrDeNt& operator=(const BazTrDeNt&) = delete;
  ~BazTrDeNt();
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazTrDeNt>(), "");

struct BazNtTrTr {
  BazNtTrTr(const BazNtTrTr&);
  BazNtTrTr& operator=(const BazNtTrTr&) = default;
  ~BazNtTrTr() = default;
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazNtTrTr>(), "");

struct BazNtTrNt {
  BazNtTrNt(const BazNtTrNt&);
  BazNtTrNt& operator=(const BazNtTrNt&) = default;
  ~BazNtTrNt();
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazNtTrNt>(), "");

struct BazNtNtTr {
  BazNtNtTr(const BazNtNtTr&);
  BazNtNtTr& operator=(const BazNtNtTr&);
  ~BazNtNtTr() = default;
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazNtNtTr>(), "");

struct BazNtNtNt {
  BazNtNtNt(const BazNtNtNt&);
  BazNtNtNt& operator=(const BazNtNtNt&);
  ~BazNtNtNt();
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazNtNtNt>(), "");

struct BazNtDeTr {
  BazNtDeTr(const BazNtDeTr&);
  BazNtDeTr& operator=(const BazNtDeTr&) = delete;
  ~BazNtDeTr() = default;
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazNtDeTr>(), "");

struct BazNtDeNt {
  BazNtDeNt(const BazNtDeNt&);
  BazNtDeNt& operator=(const BazNtDeNt&) = delete;
  ~BazNtDeNt();
};
static_assert(!sanitizer_impl::IsTriviallyCopyable<BazNtDeNt>(), "");

// Trivially copyable types.

struct Foo {
  uint32_t field1;
  uint16_t field2;
};

struct Bar {
  uint32_t ID;
  Foo foo;
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

// TODO(b/9116): Enable the test when the bug is fixed.
TEST(SanitizerTest, DISABLED_MsanUninitialized) {
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
