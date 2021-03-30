/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TIMER_FAKE_TIMEOUT_H_
#define NET_DCSCTP_TIMER_FAKE_TIMEOUT_H_

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "net/dcsctp/public/timeout.h"

namespace dcsctp {

// A timeout used in tests.
class FakeTimeout : public Timeout {
 public:
  explicit FakeTimeout(std::function<int64_t()> get_time_ms,
                       std::function<void(FakeTimeout*)> on_delete)
      : get_time_ms_(std::move(get_time_ms)),
        on_delete_(std::move(on_delete)) {}

  ~FakeTimeout() override { on_delete_(this); }

  void Start(int duration_ms, TimeoutID timeout_id) override {
    timeout_id_ = timeout_id;
    expiry_ = get_time_ms_() + duration_ms;
  }
  void Stop() override { expiry_ = kInfiniteFuture; }

  bool EvaluateHasExpired(int64_t now) {
    if (now >= expiry_) {
      expiry_ = kInfiniteFuture;
      return true;
    }
    return false;
  }

  TimeoutID timeout_id() const { return timeout_id_; }

 private:
  static constexpr int64_t kInfiniteFuture =
      std::numeric_limits<int64_t>::max();

  const std::function<int64_t()> get_time_ms_;
  const std::function<void(FakeTimeout*)> on_delete_;

  TimeoutID timeout_id_ = TimeoutID(0);
  int64_t expiry_ = kInfiniteFuture;
};

class FakeTimeoutManager {
 public:
  explicit FakeTimeoutManager(std::function<int64_t()> get_time_ms)
      : get_time_ms_(std::move(get_time_ms)) {}

  std::unique_ptr<Timeout> CreateTimeout() {
    auto timer = std::make_unique<FakeTimeout>(
        get_time_ms_, [this](FakeTimeout* timer) { timers_.erase(timer); });
    timers_.insert(timer.get());
    return timer;
  }

  std::vector<TimeoutID> RunTimers() {
    int64_t now = get_time_ms_();
    std::vector<TimeoutID> expired_timers;
    for (auto& timer : timers_) {
      if (timer->EvaluateHasExpired(now)) {
        expired_timers.push_back(timer->timeout_id());
      }
    }
    return expired_timers;
  }

 private:
  const std::function<int64_t()> get_time_ms_;
  std::unordered_set<FakeTimeout*> timers_;
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_TIMER_FAKE_TIMEOUT_H_
