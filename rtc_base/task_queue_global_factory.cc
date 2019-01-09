/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue_global_factory.h"

#include "api/task_queue/task_queue_default_factory.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

TaskQueueFactory* GlobalOrDefault(TaskQueueFactory* global) {
  static TaskQueueFactory* const factory =
      global ? global : new DefaultTaskQueueFactory();
  return factory;
}

}  // namespace

void SetGlobalTaskQueueFactory(TaskQueueFactory* factory) {
  RTC_CHECK(factory) << "Can't set nullptr TaskQueueFactory";
  RTC_CHECK(GlobalOrDefault(factory) == factory)
      << "Task queue factory set after another SetFactory or after a task "
         "queue was created";
}

TaskQueueFactory& GlobalTaskQueueFactory() {
  TaskQueueFactory* factory = GlobalOrDefault(nullptr);
  RTC_CHECK(factory);
  return *factory;
}

}  // namespace webrtc
