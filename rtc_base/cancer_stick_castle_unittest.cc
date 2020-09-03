/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
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
class Foo {
 public:
  void Increment(int* i) { (*i)++; }
};

TEST(CancerStickCastle, NoRecieverSingleMessegeTest) {
  webrtc::CancerStickCastle<std::string> node;

  node.Send("messege");
}

TEST(CancerStickCastle, NoRecieverMultipleMessegeTest) {
  webrtc::CancerStickCastle<std::string, int> node;

  node.Send("messege0", 123);
}

TEST(CancerStickCastle, OneReceiverInvokedWithReference) {
  webrtc::CancerStickCastle<int&> c;
  rtc::FunctionView<void(int&)> fvTest([](int& index) { index++; });
  c.AddReceiver(fvTest);
  int index = 1;

  c.Send(index);

  EXPECT_EQ(index, 2);
}

TEST(CancerStickCastle, OneReceiverInvokedWithPointer) {
  webrtc::CancerStickCastle<int*> c;
  rtc::FunctionView<void(int*)> fvTest([](int* index) { (*index)++; });
  c.AddReceiver(fvTest);
  int index = 1;

  c.Send(&index);

  EXPECT_EQ(index, 2);
}

TEST(CancerStickCastle, NonTrivialCollableTest) {
  webrtc::CancerStickCastle<int*> c;
  int start = 5;
  c.AddReceiver([start](int* index) { (*index) += start; });
  c.AddReceiver([](int* index) { (*index)++; });
  int index = 1;

  c.Send(&index);

  EXPECT_EQ(index, 7);
}

// todo(glahiru): Add a test case to catch some error for Karl's first fix
// which used the following code in the Send
// f.Call<void(ArgT...), ArgT...
}  // namespace rtc
