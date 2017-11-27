/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_REGISTER_TOKEN_H_
#define MODULES_RTP_RTCP_SOURCE_REGISTER_TOKEN_H_

#include <memory>
#include <utility>

#include "modules/rtp_rtcp/source/register_token_internal.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

// TODO(danilchap): publish in api/ or rtc_base/ if this pattern turns to be
// comfortable to read and use.

// Move-only class that runs unregister() exactly once (on destruction or when
// reset function is called).
class RegisterToken {
 public:
  // Runs reg() to the task_queue. returns token that can be reset/destroyed on
  // any thread to destroy token returned by reg on the task_queue.
  // task_queue must outlive returned RegisterToken.
  template <typename RegisterClosure>
  static RegisterToken CreateOnTaskQueue(rtc::TaskQueue* task_queue,
                                         RegisterClosure&& reg) {
    return RegisterToken(register_token_internal::RegisterOnTaskQueue(
        task_queue, std::forward<RegisterClosure>(reg)));
  }

  RegisterToken() = default;
  RegisterToken(RegisterToken&&) = default;
  template <typename Closure>
  explicit RegisterToken(Closure&& unregister)
      : unregister_(register_token_internal::MakeCleanupClosure(
            std::forward<Closure>(unregister))) {}
  RegisterToken& operator=(RegisterToken&&) = default;
  ~RegisterToken() = default;

  void Clear() { unregister_.reset(); }

 private:
  explicit RegisterToken(
      std::unique_ptr<register_token_internal::UnregisterInterface> unregister)
      : unregister_(std::move(unregister)) {}

  std::unique_ptr<register_token_internal::UnregisterInterface> unregister_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_REGISTER_TOKEN_H_
