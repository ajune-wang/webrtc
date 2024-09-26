/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/scoped_refptr.h"

#include <type_traits>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

struct FunctionsCalled {
  int addref = 0;
  int release = 0;
};

class ScopedRefCounted {
 public:
  explicit ScopedRefCounted(FunctionsCalled* called) : called_(*called) {}
  ScopedRefCounted(const ScopedRefCounted&) = delete;
  ScopedRefCounted& operator=(const ScopedRefCounted&) = delete;

  void AddRef() {
    ++called_.addref;
    ++ref_count_;
  }
  void Release() {
    ++called_.release;
    if (0 == --ref_count_)
      delete this;
  }

 private:
  ~ScopedRefCounted() = default;

  FunctionsCalled& called_;
  int ref_count_ = 0;
};

TEST(ScopedRefptrTest, IsCopyConstructable) {
  FunctionsCalled called;
  scoped_refptr<ScopedRefCounted> ptr(new ScopedRefCounted(&called));
  scoped_refptr<ScopedRefCounted> another_ptr = ptr;

  EXPECT_TRUE(ptr);
  EXPECT_TRUE(another_ptr);
  EXPECT_EQ(called.addref, 2);
}

TEST(ScopedRefptrTest, IsCopyAssignable) {
  FunctionsCalled called;
  scoped_refptr<ScopedRefCounted> another_ptr;
  scoped_refptr<ScopedRefCounted> ptr(new ScopedRefCounted(&called));
  another_ptr = ptr;

  EXPECT_TRUE(ptr);
  EXPECT_TRUE(another_ptr);
  EXPECT_EQ(called.addref, 2);
}

TEST(ScopedRefptrTest, IsMoveConstructableWithoutExtraAddRefRelease) {
  FunctionsCalled called;
  scoped_refptr<ScopedRefCounted> ptr(new ScopedRefCounted(&called));
  scoped_refptr<ScopedRefCounted> another_ptr = std::move(ptr);

  EXPECT_FALSE(ptr);
  EXPECT_TRUE(another_ptr);
  EXPECT_EQ(called.addref, 1);
  EXPECT_EQ(called.release, 0);
}

TEST(ScopedRefptrTest, IsMoveAssignableWithoutExtraAddRefRelease) {
  FunctionsCalled called;
  scoped_refptr<ScopedRefCounted> another_ptr;
  scoped_refptr<ScopedRefCounted> ptr(new ScopedRefCounted(&called));
  another_ptr = std::move(ptr);

  EXPECT_FALSE(ptr);
  EXPECT_TRUE(another_ptr);
  EXPECT_EQ(called.addref, 1);
  EXPECT_EQ(called.release, 0);
}

TEST(ScopedRefptrTest, MovableDuringVectorReallocation) {
  static_assert(
      std::is_nothrow_move_constructible<scoped_refptr<ScopedRefCounted>>(),
      "");
  // Test below describes a scenario where it is helpful for move constructor
  // to be noexcept.
  FunctionsCalled called;
  std::vector<scoped_refptr<ScopedRefCounted>> ptrs;
  ptrs.reserve(1);
  // Insert more elements than reserved to provoke reallocation.
  ptrs.emplace_back(new ScopedRefCounted(&called));
  ptrs.emplace_back(new ScopedRefCounted(&called));

  EXPECT_EQ(called.addref, 2);
  EXPECT_EQ(called.release, 0);
}

TEST(ScopedRefptrTest, SupportsNullablityAnnotation) {
  FunctionsCalled called;
  // Expect this test compiles.
  absl::Nonnull<scoped_refptr<ScopedRefCounted>> a =
      make_ref_counted<ScopedRefCounted>(&called);
  absl::Nullable<scoped_refptr<ScopedRefCounted>> b;
}

class A : public RefCountInterface {};
class B : public RefCountInterface {};
class DerivedFromA : public A {};

class UsesOverlads {
 public:
  void Set(scoped_refptr<A> value) { a_ = std::move(value); }
  void Set(scoped_refptr<B> value) { b_ = std::move(value); }

  A* a() const { return a_.get(); }
  B* b() const { return b_.get(); }

 private:
  scoped_refptr<A> a_;
  scoped_refptr<B> b_;
};

TEST(ScopedRefptrTest, SupportsOverloadResolution) {
  UsesOverlads test;
  scoped_refptr<A> a = make_ref_counted<A>();
  scoped_refptr<B> b = make_ref_counted<B>();
  scoped_refptr<DerivedFromA> derived_from_a = make_ref_counted<DerivedFromA>();

  test.Set(a);
  EXPECT_EQ(test.a(), a.get());

  test.Set(b);
  EXPECT_EQ(test.b(), b.get());

  test.Set(derived_from_a);
  EXPECT_EQ(test.a(), derived_from_a.get());
}

}  // namespace
}  // namespace webrtc
