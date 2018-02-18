/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SINGLE_THREADED_TASK_QUEUE_H_
#define TEST_SINGLE_THREADED_TASK_QUEUE_H_

#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"

namespace webrtc {
namespace test {

using SingleThreadedTaskQueueForTesting = rtc::test::TaskQueueForTest;

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SINGLE_THREADED_TASK_QUEUE_H_
