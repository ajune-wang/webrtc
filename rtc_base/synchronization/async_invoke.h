/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_SYNCHRONIZATION_ASYNC_INVOKE_H_
#define RTC_BASE_SYNCHRONIZATION_ASYNC_INVOKE_H_
#include <memory>

#include "rtc_base/constructormagic.h"
#include "rtc_base/event.h"

namespace rtc {
class InvokeDoneBlocker;
class InvokeWaiter {
 public:
  InvokeWaiter();
  ~InvokeWaiter();
  InvokeDoneBlocker CreateBlocker();
  void Wait();

 protected:
  friend class InvokeDoneBlocker;
  void AddBlock();
  void DecBlock();

 private:
  volatile int blocker_count_ = 0;
  Event done_;
  RTC_DISALLOW_COPY_AND_ASSIGN(InvokeWaiter);
};
class AutoWaiter : public InvokeWaiter {
 public:
  ~AutoWaiter();
};

class InvokeDoneBlocker {
 public:
  InvokeDoneBlocker();
  InvokeDoneBlocker(InvokeDoneBlocker&& other);
  InvokeDoneBlocker(const InvokeDoneBlocker& other);
  void operator=(const InvokeDoneBlocker& other);

  static InvokeDoneBlocker NonBlocking();

  ~InvokeDoneBlocker();
  bool IsBlocking() const;

 protected:
  friend class InvokeWaiter;
  explicit InvokeDoneBlocker(InvokeWaiter* target);

 private:
  InvokeWaiter* target_;
};
}  // namespace rtc

#endif  // RTC_BASE_SYNCHRONIZATION_ASYNC_INVOKE_H_
