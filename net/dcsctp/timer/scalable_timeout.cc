/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/timer/scalable_timeout.h"

#include <array>
#include <cstdint>

#include "rtc_base/synchronization/mutex.h"

namespace dcsctp {

constexpr DurationMs ScalableTimeoutDriver::kResolution;
constexpr size_t ScalableTimeoutDriver::kBucketCount;

void ScalableTimeout::Unlink() {
  if (prevs_next_ != nullptr) {
    *prevs_next_ = next_;
    if (next_ != nullptr) {
      next_->prevs_next_ = prevs_next_;
    }
    prevs_next_ = nullptr;
    next_ = nullptr;
  }
}

void ScalableTimeout::InsertAt(ScalableTimeout** list_head) {
  if (*list_head != nullptr) {
    (*list_head)->prevs_next_ = &next_;
  }
  prevs_next_ = list_head;
  next_ = *list_head;
  *list_head = this;
}

ScalableTimeout::~ScalableTimeout() {
  driver_.Cancel(this);
}

void ScalableTimeoutDriver::ScheduleLocked(ScalableTimeout* timer,
                                           TimeMs expiry,
                                           TimeoutID timeout_id) {
  int ticks = (expiry - last_tick_) / kResolution;
  int bucket = (last_tick_position_ + ticks) % kBucketCount;
  timer->revolutions_ = ticks / kBucketCount;
  timer->timeout_id_ = timeout_id;
  timer->InsertAt(&buckets_[bucket]);
}

void ScalableTimeoutDriver::Schedule(ScalableTimeout* timer,
                                     DurationMs duration,
                                     TimeoutID timeout_id) {
  TimeMs expiry = get_time_() + duration;
  webrtc::MutexLock lock(&mutex_);
  ScheduleLocked(timer, expiry, timeout_id);
}

void ScalableTimeoutDriver::Cancel(ScalableTimeout* timer) {
  webrtc::MutexLock lock(&mutex_);
  timer->Unlink();
}

void ScalableTimeoutDriver::Reschedule(ScalableTimeout* timer,
                                       DurationMs duration,
                                       TimeoutID timeout_id) {
  TimeMs expiry = get_time_() + duration;
  webrtc::MutexLock lock(&mutex_);
  timer->Unlink();
  ScheduleLocked(timer, expiry, timeout_id);
}

void ScalableTimeoutDriver::Tick() {
  webrtc::MutexLock lock(&mutex_);
  TimeMs now = get_time_();
  while (last_tick_ < now) {
    last_tick_ = last_tick_ + kResolution;
    ++last_tick_position_;
    int position = last_tick_position_ % kBucketCount;

    for (ScalableTimeout* it = buckets_[position]; it != nullptr;
         it = it->next_) {
      if (it->revolutions_ == 0) {
        it->Unlink();
        it->factory_.on_timeout_expired_(it->timeout_id());
      } else {
        --it->revolutions_;
      }
    }
  }
}

std::unique_ptr<Timeout> ScalableTimeoutFactory::CreateTimeout() {
  return std::make_unique<ScalableTimeout>(&driver_, this);
}

}  // namespace dcsctp
