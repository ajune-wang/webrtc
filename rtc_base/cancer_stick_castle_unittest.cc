/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <string>

#include "api/function_view.h"
#include "rtc_base/bind.h"
#include "rtc_base/cancer_stick_castle.h"
#include "test/gtest.h"

namespace rtc {
TEST(CancerStickCastle, NoRecieverSingleMessageTest) {
  webrtc::CancerStickCastle<std::string> c;

  c.Send("message");
}

TEST(CancerStickCastle, MultipleParameterMessageTest) {
  webrtc::CancerStickCastle<const std::string&, std::string, std::string&&, int,
                            int*, std::string&>
      c;
  std::string str = "messege";
  int i = 10;

  c.Send(str, "message1", "message0", 123, &i, str);
}

TEST(CancerStickCastle, NoParameterMessageTest) {
  webrtc::CancerStickCastle<> c;

  c.Send();
}

TEST(CancerStickCastle, ReferenceTest) {
  webrtc::CancerStickCastle<int&> c;
  int index = 1;

  c.AddReceiver([](int& index) { index++; });
  c.Send(index);

  EXPECT_EQ(index, 2);
}

TEST(CancerStickCastle, ConstReferenceTest) {
  webrtc::CancerStickCastle<int&> c;
  int i = 0;
  int index = 1;

  c.AddReceiver([&](const int& index) mutable { i = index; });
  c.Send(index);

  EXPECT_EQ(i, 1);
}

TEST(CancerStickCastle, FunctionPtrTest) {
  webrtc::CancerStickCastle<int&> c;
  auto increment = [](int& index) { index++; };
  int index = 1;

  c.AddReceiver(increment);
  c.Send(index);

  EXPECT_EQ(index, 2);
}

TEST(CancerStickCastle, PointerTest) {
  webrtc::CancerStickCastle<int*> c;
  int index = 1;

  c.AddReceiver([](int* index) { (*index)++; });
  c.Send(&index);

  EXPECT_EQ(index, 2);
}

void PlusOne(int& a) {
  a++;
}

TEST(CancerStickCastle, SmallNonTrivialTest) {
  webrtc::CancerStickCastle<int&> c;
  std::function<void(int&)> smallFunc = PlusOne;
  int index = 1;

  c.AddReceiver(smallFunc);
  c.Send(index);

  EXPECT_EQ(index, 2);
}

/* sizeof(LargeTrivial) = 20bytes which is greater than
 * the size check (16bytes) of the CSC library */
struct LargeTrivial {
  int a, b, c, d, e;
  void operator()() { a = 10; }
};

TEST(CancerStickCastle, LargeTrivial) {
  webrtc::CancerStickCastle<> c;
  LargeTrivial l = LargeTrivial();
  LargeTrivial& r = l;

  c.AddReceiver(r);
  c.Send();
}

struct OnlyNonTriviallyConstructible {
  int a;
  OnlyNonTriviallyConstructible() = default;
  OnlyNonTriviallyConstructible(OnlyNonTriviallyConstructible& m) {}
  OnlyNonTriviallyConstructible(OnlyNonTriviallyConstructible&& m) {}
  void operator()() { a = 10; }
};

TEST(CancerStickCastle, OnlyNonTriviallyMoveConstructible) {
  webrtc::CancerStickCastle<> c;
  OnlyNonTriviallyConstructible l = OnlyNonTriviallyConstructible();

  c.AddReceiver(l);
  c.Send();
}

TEST(CancerStickCastle, MultipleReceiverSendTest) {
  webrtc::CancerStickCastle<int&> c;
  std::function<void(int&)> plus = PlusOne;
  int index = 1;

  c.AddReceiver(plus);
  c.AddReceiver([](int& i) { i--; });
  c.AddReceiver(plus);
  c.AddReceiver(plus);
  c.Send(index);
  c.Send(index);

  EXPECT_EQ(index, 5);
}
// todo(glahiru): Add a test to test function pointers
// todo(glahiru): Add a test case to catch some error for Karl's first fix
// todo(glahiru): Add a test for rtc::Bind
// which used the following code in the Send
// f.Call<void(ArgT...), ArgT...
}  // namespace rtc
