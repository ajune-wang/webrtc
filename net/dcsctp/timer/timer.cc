/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/timer/timer.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "net/dcsctp/public/timeout.h"

namespace dcsctp {
namespace {
TimeoutID MakeTimeoutId(uint32_t timer_id, uint32_t generation) {
  return TimeoutID(static_cast<uint64_t>(timer_id) << 32 | generation);
}

int64_t GetBackoffDuration(TimerBackoffAlgorithm algorithm,
                           int64_t base_duration_ms,
                           int expiration_count) {
  switch (algorithm) {
    case TimerBackoffAlgorithm::kFixed:
      return base_duration_ms;
    case TimerBackoffAlgorithm::kExponential:
      return base_duration_ms * (1 << expiration_count);
  }
}
}  // namespace

Timer::Timer(uint32_t id,
             absl::string_view name,
             OnExpired on_expired,
             UnregisterHandler unregister_handler,
             std::unique_ptr<Timeout> timeout,
             const TimerOptions& options)
    : id_(id),
      name_(name),
      options_(options),
      on_expired_(std::move(on_expired)),
      unregister_handler_(std::move(unregister_handler)),
      timeout_(std::move(timeout)),
      duration_ms_(options.duration_ms) {}

Timer::~Timer() {
  Stop();
  unregister_handler_();
}

void Timer::Start() {
  if (!is_running()) {
    is_running_ = true;
    expiration_count_ = 0;
    timeout_->Start(duration_ms_, MakeTimeoutId(id_, ++generation_));
  }
}

void Timer::Stop() {
  if (is_running()) {
    timeout_->Stop();
    expiration_count_ = 0;
    is_running_ = false;
  }
}

void Timer::Restart() {
  expiration_count_ = 0;
  if (!is_running()) {
    is_running_ = true;
    timeout_->Start(duration_ms_, MakeTimeoutId(id_, ++generation_));
  } else {
    // Timer was running - emulate an atomic stop and start.
    timeout_->Restart(duration_ms_, MakeTimeoutId(id_, ++generation_));
  }
}

void Timer::Trigger(uint32_t generation) {
  if (is_running_ && generation == generation_) {
    ++expiration_count_;
    if (options_.max_restarts >= 0 &&
        expiration_count_ > options_.max_restarts) {
      is_running_ = false;
    }

    absl::optional<int> new_duration_ms = on_expired_();
    if (new_duration_ms.has_value()) {
      duration_ms_ = new_duration_ms.value();
    }

    if (is_running_) {
      // Restart it with new duration.
      int64_t duration = GetBackoffDuration(options_.backoff_algorithm,
                                            duration_ms_, expiration_count_);
      timeout_->Start(duration, MakeTimeoutId(id_, ++generation_));
    }
  }
}

void TimerManager::HandleTimeout(TimeoutID timeout_id) {
  uint32_t timer_id = *timeout_id >> 32;
  uint32_t generation = *timeout_id;
  auto it = timers_.find(timer_id);
  if (it != timers_.end()) {
    it->second->Trigger(generation);
  }
}

std::unique_ptr<Timer> TimerManager::CreateTimer(absl::string_view name,
                                                 Timer::OnExpired on_expired,
                                                 const TimerOptions& options) {
  uint32_t id = ++next_id_;
  auto timer = absl::WrapUnique(new Timer(
      id, name, std::move(on_expired), [this, id]() { timers_.erase(id); },
      create_timeout_(), options));
  timers_[id] = timer.get();
  return timer;
}

}  // namespace dcsctp
