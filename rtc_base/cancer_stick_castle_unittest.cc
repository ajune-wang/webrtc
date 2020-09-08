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
  webrtc::CancerStickCastle<std::string, int> c;

  c.Send("message0", 123);
}

TEST(CancerStickCastle, NoParameterMessageTest) {
  webrtc::CancerStickCastle<> c;

  c.Send();
}

TEST(CancerStickCastle, OneReceiverInvokedWithReference) {
  webrtc::CancerStickCastle<int&> c;
  c.AddReceiver([](int& index) { index++; });
  int index = 1;

  c.Send(index);

  EXPECT_EQ(index, 2);
}

TEST(CancerStickCastle, FunctionPtrTest) {
  webrtc::CancerStickCastle<int&> c;
  auto increment = [](int& index) { index++; };
  c.AddReceiver(increment);
  int index = 1;

  c.Send(index);

  EXPECT_EQ(index, 2);
}

TEST(CancerStickCastle, OneReceiverInvokedWithPointer) {
  webrtc::CancerStickCastle<int*> c;
  c.AddReceiver([](int* index) { (*index)++; });
  int index = 1;

  c.Send(&index);

  EXPECT_EQ(index, 2);
}

TEST(CancerStickCastle, NonTrivialCallableTest) {
  webrtc::CancerStickCastle<int&> c;
  int start = 5;
  c.AddReceiver([start](int& index) { index += start; });
  int index = 1;

  c.Send(index);

  EXPECT_EQ(index, 6);
}

void addAll(int* a, int* b, int* c, int* d) {
  *a = (*a) + (*b) + (*c) + (*d);
}

TEST(CancerStickCastle, LargeCallableTest) {
  webrtc::CancerStickCastle<int*, int*, int*, int*> c;
  // Below method has the sizeof 32bytes which is bigger
  // than the size check in the library (16bytes).
  std::function<void(int*, int*, int*, int*)> f1 = addAll;
  c.AddReceiver(f1);
  int index = 1;

  c.Send(&index, &index, &index, &index);

  EXPECT_EQ(index, 4);
}
// todo(glahiru): Add a test to test function pointers
// todo(glahiru): Add a test case to catch some error for Karl's first fix
// which used the following code in the Send
// f.Call<void(ArgT...), ArgT...
}  // namespace rtc
