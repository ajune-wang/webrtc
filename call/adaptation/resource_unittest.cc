/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource.h"

#include <memory>

#include "api/scoped_refptr.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_factory.h"
#include "call/adaptation/test/fake_resource.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::StrictMock;

class MockResourceListener : public ResourceListener {
 public:
  MOCK_METHOD(void,
              OnResourceUsageStateMeasured,
              (rtc::scoped_refptr<Resource> resource));
};

class ResourceTest : public ::testing::Test {
 public:
  ResourceTest()
      : task_queue_factory_(CreateDefaultTaskQueueFactory()),
        resource_adaptation_queue_(task_queue_factory_->CreateTaskQueue(
            "ResourceAdaptationQueue",
            TaskQueueFactory::Priority::NORMAL)),
        encoder_queue_(task_queue_factory_->CreateTaskQueue(
            "EncoderQueue",
            TaskQueueFactory::Priority::NORMAL)),
        fake_resource_(new FakeResource("FakeResource")) {
    fake_resource_->Initialize(&encoder_queue_, &resource_adaptation_queue_);
  }

 protected:
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  rtc::TaskQueue resource_adaptation_queue_;
  rtc::TaskQueue encoder_queue_;
  rtc::scoped_refptr<FakeResource> fake_resource_;
};

TEST_F(ResourceTest, RegisteringListenerReceivesCallbacks) {
  rtc::Event event;
  resource_adaptation_queue_.PostTask([this, &event] {
    StrictMock<MockResourceListener> resource_listener;
    fake_resource_->SetResourceListener(&resource_listener);
    EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_))
        .Times(1)
        .WillOnce([](rtc::scoped_refptr<Resource> resource) {
          EXPECT_EQ(ResourceUsageState::kOveruse, resource->usage_state());
        });
    fake_resource_->set_usage_state(ResourceUsageState::kOveruse);
    fake_resource_->SetResourceListener(nullptr);
    event.Set();
  });
  event.Wait(rtc::Event::kForever);
}

TEST_F(ResourceTest, UnregisteringListenerStopsCallbacks) {
  rtc::Event event;
  resource_adaptation_queue_.PostTask([this, &event] {
    StrictMock<MockResourceListener> resource_listener;
    fake_resource_->SetResourceListener(&resource_listener);
    fake_resource_->SetResourceListener(nullptr);
    EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_)).Times(0);
    fake_resource_->set_usage_state(ResourceUsageState::kOveruse);
    event.Set();
  });
  event.Wait(rtc::Event::kForever);
}

}  // namespace webrtc
