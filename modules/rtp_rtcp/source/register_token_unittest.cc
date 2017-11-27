/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/register_token.h"

#include <list>
#include <utility>

#include "rtc_base/event.h"
#include "rtc_base/weak_ptr.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace {

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;
using ::webrtc::RegisterToken;

class MockResource {
 public:
  MOCK_METHOD0(OnRegister, void());
  MOCK_METHOD0(OnUnregister, void());
  MOCK_METHOD0(OnCall, void());
};

RegisterToken Register(MockResource* mock) {
  mock->OnRegister();
  return RegisterToken(/*unregister=*/[mock] { mock->OnUnregister(); });
}

void WaitPostedTasks(rtc::TaskQueue* queue) {
  rtc::Event done(false, false);
  queue->PostTask([&done] { done.Set(); });
  ASSERT_TRUE(done.Wait(1000));
}

TEST(RegisterTokenTest, RunsUnregisterOnDestruction) {
  StrictMock<MockResource> mock;
  EXPECT_CALL(mock, OnRegister());
  RegisterToken token = Register(&mock);
  EXPECT_CALL(mock, OnUnregister());
}

TEST(RegisterTokenTest, RunsUnregisterOnClear) {
  StrictMock<MockResource> mock;
  EXPECT_CALL(mock, OnRegister());
  RegisterToken token = Register(&mock);

  EXPECT_CALL(mock, OnUnregister());
  token.Clear();
  Mock::VerifyAndClearExpectations(&mock);  // Before token is destroyed.
}

TEST(RegisterTokenTest, MovePassUnregisterResponsiblity) {
  StrictMock<MockResource> mock;
  EXPECT_CALL(mock, OnRegister());
  RegisterToken token = Register(&mock);

  EXPECT_CALL(mock, OnUnregister()).Times(0);
  RegisterToken token2 = std::move(token);
  Mock::VerifyAndClearExpectations(&mock);

  EXPECT_CALL(mock, OnUnregister());
  token2.Clear();
}

TEST(RegisterTokenCreateOnTaskQueueTest, RunsRegisterAndUnregisterOnTaskQueue) {
  StrictMock<MockResource> mock;
  rtc::TaskQueue task_queue("task_queue");
  EXPECT_CALL(mock, OnRegister()).WillOnce(Invoke([&task_queue] {
    EXPECT_TRUE(task_queue.IsCurrent());
  }));
  RegisterToken safe_token = RegisterToken::CreateOnTaskQueue(
      &task_queue, [&mock] { return Register(&mock); });
  WaitPostedTasks(&task_queue);
  Mock::VerifyAndClearExpectations(&mock);

  EXPECT_CALL(mock, OnUnregister()).WillOnce(Invoke([&task_queue] {
    EXPECT_TRUE(task_queue.IsCurrent());
  }));
  safe_token.Clear();
  WaitPostedTasks(&task_queue);
}

TEST(RegisterTokenCreateOnTaskQueueTest, CanTriggerUnregisterBeforeRegister) {
  StrictMock<MockResource> mock;
  rtc::Event blocker(false, false);
  rtc::TaskQueue task_queue("task_queue");
  task_queue.PostTask([&blocker] { blocker.Wait(rtc::Event::kForever); });

  RegisterToken safe_token = RegisterToken::CreateOnTaskQueue(
      &task_queue, [&mock] { return Register(&mock); });
  // queue is blocked, so register haven't fully run yet.
  safe_token.Clear();

  EXPECT_CALL(mock, OnRegister());
  EXPECT_CALL(mock, OnUnregister());
  blocker.Set();
  WaitPostedTasks(&task_queue);
}

// Register observers pattern.
class ObserverKeeper {
 public:
  ObserverKeeper() : this_(this) {}
  ~ObserverKeeper() {
    for (MockResource* observer : observers_)
      observer->OnUnregister();
  }

  RegisterToken AddObserver(MockResource* resource) {
    rtc::WeakPtr<ObserverKeeper> me = this_.GetWeakPtr();
    auto iterator = observers_.insert(observers_.end(), resource);
    resource->OnRegister();
    return RegisterToken([me, iterator] {
      if (me) {
        (*iterator)->OnUnregister();
        me->observers_.erase(iterator);
      }
    });
  }

  void CallAll() {
    for (MockResource* observer : observers_)
      observer->OnCall();
  }

 private:
  // Use list for iterator stability.
  std::list<MockResource*> observers_;
  rtc::WeakPtrFactory<ObserverKeeper> this_;
};

TEST(RegisterObserverTest, CallAll) {
  StrictMock<MockResource> mock1;
  StrictMock<MockResource> mock2;
  ObserverKeeper keeper;
  EXPECT_CALL(mock1, OnRegister());
  RegisterToken token1 = keeper.AddObserver(&mock1);
  EXPECT_CALL(mock2, OnRegister());
  RegisterToken token2 = keeper.AddObserver(&mock2);
  EXPECT_CALL(mock1, OnCall());
  EXPECT_CALL(mock2, OnCall());
  keeper.CallAll();

  EXPECT_CALL(mock2, OnUnregister());
  token2.Clear();
  EXPECT_CALL(mock1, OnCall());
  EXPECT_CALL(mock2, OnCall()).Times(0);
  keeper.CallAll();

  EXPECT_CALL(mock1, OnUnregister());
  token1.Clear();
  EXPECT_CALL(mock1, OnCall()).Times(0);
  EXPECT_CALL(mock2, OnCall()).Times(0);
  keeper.CallAll();
}

TEST(RegisterObserverTest, UnregisterImmidiatlyIfReturnResultIgnored) {
  StrictMock<MockResource> mock;
  ObserverKeeper keeper;
  EXPECT_CALL(mock, OnRegister());
  EXPECT_CALL(mock, OnUnregister());
  keeper.AddObserver(&mock);
  Mock::VerifyAndClearExpectations(&mock);  // Before keeper is destroyed.
}

TEST(RegisterObserverTest, CanUnregisterAfterKeeperDestroyed) {
  StrictMock<MockResource> mock1;
  RegisterToken token;
  {
    ObserverKeeper keeper;
    EXPECT_CALL(mock1, OnRegister());
    token = keeper.AddObserver(&mock1);
    // Keeper notify mocks on own destruction.
    EXPECT_CALL(mock1, OnUnregister());
  }
  // Shouldn't crash.
  token.Clear();
}

}  // namespace
