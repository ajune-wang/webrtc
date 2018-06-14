/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/array.h"
#include "rtc_base/gunit.h"

TEST(ArrayTest, Capacity) {
  rtc::Array<int, 10> array;
  EXPECT_EQ(array.capacity(), 10UL);
}

TEST(ArrayTest, PushBack) {
  struct TrueOnCopyAssing {
    explicit TrueOnCopyAssing(bool* copied) : copied(copied) {}
    TrueOnCopyAssing& operator=(const TrueOnCopyAssing& other) {
      *other.copied = true;
      return *this;
    }
    bool* copied;
  };

  bool copied = false;
  TrueOnCopyAssing copy_me(&copied);

  rtc::Array<TrueOnCopyAssing, 10> array;
  array.push_back(copy_me);
  EXPECT_TRUE(copied);
}

TEST(ArrayTest, PushBackRValue) {
  struct TrueOnMoveAssign {
    explicit TrueOnMoveAssign(bool* moved) : moved(moved) {}
    TrueOnMoveAssign& operator=(TrueOnMoveAssign&& other) {
      *other.moved = true;
      return *this;
    }
    bool* moved;
  };

  bool moved = false;
  TrueOnMoveAssign move_me(&moved);

  rtc::Array<TrueOnMoveAssign, 10> array;
  array.push_back(std::move(move_me));
  EXPECT_TRUE(moved);
}

TEST(ArrayTest, EmplaceBack) {
  struct NonCopyable {
    explicit NonCopyable(int value) : value(value) {}
    NonCopyable& operator=(NonCopyable&& other) = delete;
    NonCopyable& operator=(const NonCopyable& other) = delete;
    int value;
  };

  rtc::Array<NonCopyable, 10> array;
  array.emplace_back(12);
  EXPECT_EQ(array[0].value, 12);
}

TEST(ArrayTest, DestroyElementsOnClear) {
  struct IncrementOnDtor {
    explicit IncrementOnDtor(int* dtor_counter) : dtor_counter(dtor_counter) {}
    ~IncrementOnDtor() { *dtor_counter += 1; }
    int* dtor_counter;
  };

  int dtor_counter = 0;
  rtc::Array<IncrementOnDtor, 10> array;
  array.emplace_back(&dtor_counter);
  array.emplace_back(&dtor_counter);
  array.emplace_back(&dtor_counter);
  array.clear();
  EXPECT_EQ(dtor_counter, 3);
}

TEST(ArrayTest, DestroyElementsOnDestruction) {
  struct IncrementOnDtor {
    explicit IncrementOnDtor(int* dtor_counter) : dtor_counter(dtor_counter) {}
    ~IncrementOnDtor() { *dtor_counter += 1; }
    int* dtor_counter;
  };

  int dtor_counter = 0;
  {
    rtc::Array<IncrementOnDtor, 10> array;
    array.emplace_back(&dtor_counter);
    array.emplace_back(&dtor_counter);
    array.emplace_back(&dtor_counter);
  }
  EXPECT_EQ(dtor_counter, 3);
}

TEST(ArrayTest, Empty) {
  rtc::Array<int, 1> array;
  EXPECT_TRUE(array.empty());
  array.push_back(0);
  EXPECT_FALSE(array.empty());
}

TEST(ArrayTest, Size) {
  rtc::Array<int, 1> array;
  EXPECT_EQ(array.size(), 0UL);
  array.push_back(0);
  EXPECT_EQ(array.size(), 1UL);
}

TEST(ArrayTest, Full) {
  rtc::Array<int, 1> array;
  EXPECT_FALSE(array.full());
  array.push_back(0);
  EXPECT_TRUE(array.full());
  EXPECT_EQ(array.size(), array.capacity());
}

TEST(ArrayTest, Ref) {
  rtc::Array<int, 10> array;
  array.push_back(11);
  array[0] = 22;
  EXPECT_EQ(array[0], 22);
}

TEST(ArrayTest, ConstRef) {
  rtc::Array<int, 10> array;
  const rtc::Array<int, 10>& c_array = array;
  array.push_back(11);
  EXPECT_EQ(c_array[0], 11);
}

TEST(ArrayTest, PushBackWhenFull) {
  rtc::Array<int, 1> array;
  array.push_back(1);
  EXPECT_DEATH(array.push_back(2), "");
}

TEST(ArrayTest, PushBackRValueWhenFull) {
  rtc::Array<int, 1> array;
  array.push_back(std::move(1));
  EXPECT_DEATH(array.push_back(std::move(2)), "");
}

TEST(ArrayTest, EmplaceBackWhenFull) {
  rtc::Array<int, 1> array;
  array.emplace_back(1);
  EXPECT_DEATH(array.emplace_back(2), "");
}

TEST(ArrayTest, IndexOutOfBounds) {
  rtc::Array<int, 1> array;
  EXPECT_DEATH(array[0], "");
}

TEST(ArrayTest, ConstIndexOutOfBounds) {
  rtc::Array<int, 1> array;
  const rtc::Array<int, 1>& c_array = array;
  EXPECT_DEATH(c_array[0], "");
}
