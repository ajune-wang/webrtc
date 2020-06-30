/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/multi_processor_resource_adapter.h"

#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::StrictMock;

TEST(MultiProcessorResourceAdapterTest, HelloWorld) {
  rtc::scoped_refptr<FakeResource> source_resource =
      FakeResource::Create("SourceResource");

  MultiProcessorResourceAdapter multi_processor_resource_adapter;
  source_resource->SetResourceListener(&multi_processor_resource_adapter);

  StrictMock<MockResourceListener> listener1;
  StrictMock<MockResourceListener> listener2;

  rtc::scoped_refptr<Resource> adapter1 =
      multi_processor_resource_adapter.CreateAdapter();
  adapter1->SetResourceListener(&listener1);
  rtc::scoped_refptr<Resource> adapter2 =
      multi_processor_resource_adapter.CreateAdapter();
  adapter2->SetResourceListener(&listener2);

  EXPECT_CALL(listener1, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([adapter1](rtc::scoped_refptr<Resource> resource,
                           ResourceUsageState usage_state) {
        EXPECT_EQ(adapter1, resource);
        EXPECT_EQ(ResourceUsageState::kOveruse, usage_state);
      });
  EXPECT_CALL(listener2, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([adapter2](rtc::scoped_refptr<Resource> resource,
                           ResourceUsageState usage_state) {
        EXPECT_EQ(adapter2, resource);
        EXPECT_EQ(ResourceUsageState::kOveruse, usage_state);
      });
  source_resource->SetUsageState(ResourceUsageState::kOveruse);
}

}  // namespace webrtc
