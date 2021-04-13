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

constexpr DurationMs ScalableTimeoutManager::Driver::kResolution;
constexpr size_t ScalableTimeoutManager::Driver::kBucketCount;

void ScalableTimeoutManager::Driver::Timeout::Unlink() {
  if (prevs_next_ != nullptr) {
    *prevs_next_ = next_;
    if (next_ != nullptr) {
      next_->prevs_next_ = prevs_next_;
    }
    prevs_next_ = nullptr;
    next_ = nullptr;
  }
}

void ScalableTimeoutManager::Driver::Timeout::InsertAt(
    ScalableTimeoutManager::Driver::Timeout** list_head) {
  if (*list_head != nullptr) {
    (*list_head)->prevs_next_ = &next_;
  }
  prevs_next_ = list_head;
  next_ = *list_head;
  *list_head = this;
}

ScalableTimeoutManager::Driver::Timeout::~Timeout() {
  driver_.Cancel(this);
}

void ScalableTimeoutManager::Driver::ScheduleLocked(
    ScalableTimeoutManager::Driver::Timeout* timer,
    TimeMs expiry,
    TimeoutID timeout_id) {
  int ticks = (expiry - last_tick_) / kResolution;
  int position = (wheel_position_ + ticks) % kBucketCount;
  timer->revolutions_ = ticks / kBucketCount;
  timer->timeout_id_ = timeout_id;
  timer->InsertAt(&buckets_[position]);
}

void ScalableTimeoutManager::Driver::Schedule(
    ScalableTimeoutManager::Driver::Timeout* timer,
    DurationMs duration,
    TimeoutID timeout_id) {
  TimeMs expiry = get_time_() + duration;
  webrtc::MutexLock lock(&mutex_);
  ScheduleLocked(timer, expiry, timeout_id);
}

void ScalableTimeoutManager::Driver::Cancel(
    ScalableTimeoutManager::Driver::Timeout* timer) {
  webrtc::MutexLock lock(&mutex_);
  timer->Unlink();
}

void ScalableTimeoutManager::Driver::Reschedule(
    ScalableTimeoutManager::Driver::Timeout* timer,
    DurationMs duration,
    TimeoutID timeout_id) {
  TimeMs expiry = get_time_() + duration;
  webrtc::MutexLock lock(&mutex_);
  timer->Unlink();
  ScheduleLocked(timer, expiry, timeout_id);
}

void ScalableTimeoutManager::Driver::Tick() {
  RTC_DCHECK_RUN_ON(&driver_thread_checker_);
  TimeMs now = get_time_();
  webrtc::MutexLock lock(&mutex_);
  while (last_tick_ < now) {
    last_tick_ = last_tick_ + kResolution;
    ++wheel_position_;

    for (ScalableTimeoutManager::Driver::Timeout* it =
             buckets_[wheel_position_ % kBucketCount];
         it != nullptr; it = it->next_) {
      if (it->revolutions_ == 0) {
        it->Unlink();
        it->factory_.on_timeout_expired_(it->timeout_id());
      } else {
        --it->revolutions_;
      }
    }
  }
}

}  // namespace dcsctp
