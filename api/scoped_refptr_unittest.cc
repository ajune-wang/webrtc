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

#include "test/gmock.h"
#include "test/gtest.h"

namespace rtc {
namespace {

struct FunctionCallTracker {
  int addref = 0;
  int release = 0;
};

class RefCountedObject {
 public:
  explicit RefCountedObject(FunctionCallTracker* tracker)
      : tracker_(*tracker) {}
  RefCountedObject(const RefCountedObject&) = delete;
  RefCountedObject& operator=(const RefCountedObject&) = delete;

  void AddRef() {
    ++tracker_.addref;
    ++ref_count_;
  }
  void Release() {
    ++tracker_.release;
    if (0 == --ref_count_)
      delete this;
  }

 private:
  ~RefCountedObject() = default;

  FunctionCallTracker& tracker_;
  int ref_count_ = 0;
};

TEST(ScopedRefptrTest, IsCopyable) {
  FunctionCallTracker tracker;
  scoped_refptr<RefCountedObject> ptr = new RefCountedObject(&tracker);
  scoped_refptr<RefCountedObject> another_ptr = ptr;

  EXPECT_TRUE(ptr);
  EXPECT_TRUE(another_ptr);
  EXPECT_EQ(tracker.addref, 2);
}

TEST(ScopedRefptrTest, IsMovable) {
  FunctionCallTracker tracker;
  scoped_refptr<RefCountedObject> ptr = new RefCountedObject(&tracker);
  scoped_refptr<RefCountedObject> another_ptr = std::move(ptr);

  EXPECT_FALSE(ptr);
  EXPECT_TRUE(another_ptr);
  EXPECT_EQ(tracker.addref, 1);
}

}  // namespace
}  // namespace rtc
