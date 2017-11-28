/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_REGISTER_TOKEN_INTERNAL_H_
#define MODULES_RTP_RTCP_SOURCE_REGISTER_TOKEN_INTERNAL_H_

#include <memory>
#include <utility>

#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
class RegisterToken;

namespace register_token_internal {

class UnregisterInterface {
 public:
  virtual ~UnregisterInterface() = default;
};

template <typename Closure>
std::unique_ptr<UnregisterInterface> WrapClosure(Closure&& closure) {
  class Unregister : public UnregisterInterface {
   public:
    explicit Unregister(Closure&& closure)
        : closure_(std::forward<Closure>(closure)) {}
    ~Unregister() override { closure_(); }

   private:
    const Closure closure_;
  };
  return rtc::MakeUnique<Unregister>(std::forward<Closure>(closure));
}

class UnregisterOnTaskQueue : public UnregisterInterface {
 public:
  UnregisterOnTaskQueue(rtc::TaskQueue* task_queue,
                        std::unique_ptr<RegisterToken> token_ptr)
      : task_queue_(task_queue), token_ptr_{std::move(token_ptr)} {}
  ~UnregisterOnTaskQueue() override {
    RTC_DCHECK(token_ptr_.token);
    task_queue_->PostTask(std::move(token_ptr_));
    RTC_DCHECK(!token_ptr_.token);
  }

 private:
  struct UnregisterClosure {
    void operator()() { token.reset(); }
    std::unique_ptr<RegisterToken> token;
  };

  rtc::TaskQueue* task_queue_;
  UnregisterClosure token_ptr_;
};

// RegisterClosure has signature RegisterToken()
template <typename RegisterClosure>
std::unique_ptr<UnregisterInterface> RegisterOnTaskQueue(
    rtc::TaskQueue* task_queue,
    RegisterClosure&& reg) {
  struct RegisterTaskClosure {
    void operator()() { *token = reg(); }
    RegisterToken* const token;
    RegisterClosure reg;
  };

  auto token_ptr = rtc::MakeUnique<RegisterToken>();
  task_queue->PostTask(
      RegisterTaskClosure{token_ptr.get(), std::forward<RegisterClosure>(reg)});
  return rtc::MakeUnique<UnregisterOnTaskQueue>(task_queue,
                                                std::move(token_ptr));
}

}  // namespace register_token_internal
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_REGISTER_TOKEN_INTERNAL_H_
