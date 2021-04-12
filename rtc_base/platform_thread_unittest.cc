/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/platform_thread.h"

#include "rtc_base/event.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"

using ::testing::Invoke;
using ::testing::MockFunction;

namespace rtc {
namespace {

void NullRunFunction(void* obj) {}

// Function that sets a boolean.
void SetFlagRunFunction(void* obj) {
  bool* obj_as_bool = static_cast<bool*>(obj);
  *obj_as_bool = true;
}

void StdFunctionRunFunction(void* obj) {
  std::function<void()>* fun = static_cast<std::function<void()>*>(obj);
  (*fun)();
}

std::atomic<bool> g_flag{false};

}  // namespace

TEST(PlatformThreadTest, StartStop) {
  PlatformThread thread(&NullRunFunction, nullptr, "PlatformThreadTest");
  EXPECT_TRUE(thread.name() == "PlatformThreadTest");
  EXPECT_TRUE(thread.GetThreadRef() == 0);
  thread.Start();
  EXPECT_TRUE(thread.GetThreadRef() != 0);
  thread.Stop();
  EXPECT_TRUE(thread.GetThreadRef() == 0);
}

TEST(PlatformThreadTest, StartStop2) {
  PlatformThread thread1(&NullRunFunction, nullptr, "PlatformThreadTest1");
  PlatformThread thread2(&NullRunFunction, nullptr, "PlatformThreadTest2");
  EXPECT_TRUE(thread1.GetThreadRef() == thread2.GetThreadRef());
  thread1.Start();
  thread2.Start();
  EXPECT_TRUE(thread1.GetThreadRef() != thread2.GetThreadRef());
  thread2.Stop();
  thread1.Stop();
}

TEST(PlatformThreadTest, RunFunctionIsCalled) {
  bool flag = false;
  PlatformThread thread(&SetFlagRunFunction, &flag, "RunFunctionIsCalled");
  thread.Start();

  // At this point, the flag may be either true or false.
  thread.Stop();

  // We expect the thread to have run at least once.
  EXPECT_TRUE(flag);
}

TEST(PlatformThreadTest, JoinsThread) {
  // This test flakes on problems with the join implementation.
  EXPECT_TRUE(ThreadAttributes().joinable);
  rtc::Event event;
  MockFunction<void()> function;
  EXPECT_CALL(function, Call).WillOnce(Invoke([&] {
    webrtc::SleepMs(1000);
    event.Set();
  }));
  std::function<void()> std_function = function.AsStdFunction();
  PlatformThread thread(&StdFunctionRunFunction, &std_function, "T");
  thread.Start();
  thread.Stop();
  EXPECT_TRUE(event.Wait(/*give_up_after_ms=*/0));
}

TEST(PlatformThreadTest, StopsBeforeDetachedThread) {
  // This test flakes on problems with the detached thread implementation.
  g_flag = false;
  MockFunction<void()> function;
  rtc::Event event;
  EXPECT_CALL(function, Call).WillOnce(Invoke([&] {
    event.Set();
    webrtc::SleepMs(1000);
    g_flag = true;
  }));
  std::function<void()> std_function = function.AsStdFunction();
  PlatformThread thread(&StdFunctionRunFunction, &std_function, "T",
                        ThreadAttributes().SetDetached());
  thread.Start();
  event.Wait(Event::kForever);
  thread.Stop();
  EXPECT_FALSE(g_flag);
}

}  // namespace rtc
