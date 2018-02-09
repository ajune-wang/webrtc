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
#include "rtc_base/task_queue.h"

namespace webrtc {
namespace test {

// This class gives capabilities similar to rtc::TaskQueue, but ensures
// everything happens on the same thread. This is intended to make the
// threading model of unit-tests (specifically end-to-end tests) more closely
// resemble that of real WebRTC, thereby allowing us to replace some critical
// sections by thread-checkers.
// This task is NOT tuned for performance, but rather for simplicity.
class SingleThreadedTaskQueueForTesting : public rtc::TaskQueue {
 public:
  explicit SingleThreadedTaskQueueForTesting(const char* name)
      : rtc::TaskQueue(name) {}
  ~SingleThreadedTaskQueueForTesting() {}

  // Send one task to the queue. The function does not return until the task
  // has finished executing. No support for canceling the task.
  template <class Closure>
  void SendTask(Closure&& task) {
    rtc::Event event(false, false);
    PostTask(rtc::NewClosure(std::move(task), [&event]() { event.Set(); }));
    event.Wait(rtc::Event::kForever);
  }
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SINGLE_THREADED_TASK_QUEUE_H_
