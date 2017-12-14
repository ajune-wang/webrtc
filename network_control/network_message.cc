/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "network_control/include/network_message.h"

namespace webrtc {
namespace network {
namespace signal {
namespace internal {

PendingTaskHandler::PendingTask::PendingTask(
    closure_t&& closure,
    rtc::scoped_refptr<AliveToken> token)
    : closure_(std::forward<closure_t>(closure)), token_(token) {}
PendingTaskHandler::PendingTask::~PendingTask() {}
bool PendingTaskHandler::PendingTask::Run() {
  if (token_->alive)
    closure_();
  return true;
}

PendingTaskHandler::PendingTaskHandler(rtc::TaskQueue* target_queue)
    : target_queue_(target_queue), token_(new AliveToken()) {}

PendingTaskHandler::~PendingTaskHandler() {
  rtc::Event done(false, false);
  target_queue_->PostTask(rtc::NewClosure([this] { token_->alive = false; },
                                          [&done] { done.Set(); }));
  done.Wait(rtc::Event::kForever);
  RTC_DCHECK(!token_->alive);
}

void PendingTaskHandler::PostTask(closure_t&& closure) {
  std::unique_ptr<PendingTask> pending_task =
      rtc::MakeUnique<PendingTask>(std::move(closure), token_);
  target_queue_->PostTask(std::move(pending_task));
}

void PendingTaskHandler::PostDelayedTask(closure_t&& closure,
                                         units::TimeDelta delay) {
  std::unique_ptr<PendingTask> pending_task =
      rtc::MakeUnique<PendingTask>(std::move(closure), token_);
  target_queue_->PostDelayedTask(std::move(pending_task), delay.ms());
}
}  // namespace internal
}  // namespace signal
}  // namespace network
}  // namespace webrtc
