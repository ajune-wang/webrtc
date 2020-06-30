/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/broadcast_resource_listener.h"

#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::StrictMock;

TEST(BroadcastResourceListenerTest, HelloWorld) {
  rtc::scoped_refptr<FakeResource> source_resource =
      FakeResource::Create("SourceResource");

  BroadcastResourceListener broadcast_resource_listener(source_resource);
  source_resource->SetResourceListener(&broadcast_resource_listener);

  StrictMock<MockResourceListener> destination_listener1;
  StrictMock<MockResourceListener> destination_listener2;

  rtc::scoped_refptr<Resource> adapter1 =
      broadcast_resource_listener.CreateAdapter();
  adapter1->SetResourceListener(&destination_listener1);
  rtc::scoped_refptr<Resource> adapter2 =
      broadcast_resource_listener.CreateAdapter();
  adapter2->SetResourceListener(&destination_listener2);

  EXPECT_CALL(destination_listener1, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([adapter1](rtc::scoped_refptr<Resource> resource,
                           ResourceUsageState usage_state) {
        EXPECT_EQ(adapter1, resource);
        EXPECT_EQ(ResourceUsageState::kOveruse, usage_state);
      });
  EXPECT_CALL(destination_listener2, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([adapter2](rtc::scoped_refptr<Resource> resource,
                           ResourceUsageState usage_state) {
        EXPECT_EQ(adapter2, resource);
        EXPECT_EQ(ResourceUsageState::kOveruse, usage_state);
      });
  source_resource->SetUsageState(ResourceUsageState::kOveruse);
}

}  // namespace webrtc
