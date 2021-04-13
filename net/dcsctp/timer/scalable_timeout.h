/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TIMER_SCALABLE_TIMEOUT_H_
#define NET_DCSCTP_TIMER_SCALABLE_TIMEOUT_H_

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "net/dcsctp/public/timeout.h"
#include "rtc_base/synchronization/mutex.h"

namespace dcsctp {

class ScalableTimeout;

// A scalable timeout driver
//
// This class should be managed as a singleton - there should be one per
// application. This class should have its own dedicated thread which calls
// `Tick` at an even pace, decided by `kResolution`.
//
// This implementation is scalable in that it can efficiently handle timeouts
// from thousands of sockets which resolve to many thousand timeouts. They can
// be efficiently started and stopped with no measurable contention. To achieve
// this, the timeout duration is only as precise as the resolution, as decided
// by `kResolution` and it will need to be continuously driven by calling
// `Tick`, which may be something to avoided in some environments.
//
// The actual implementation is a non-hierarchical hashed timing wheel with each
// bucket maintaining an asymmetric doubly-linked list of timers, allowing O(1)
// Start/Stop/Restart and very efficient timer evaluation.
//
// This class is thread-safe.
class ScalableTimeoutDriver {
 public:
  // The timer resolution, in milliseconds.
  static constexpr DurationMs kResolution = DurationMs(10);
  // The number of buckets in the timer wheel.
  static constexpr size_t kBucketCount = 256;

  // Instantiates a ScalableTimeoutDriver, which manages all created timeouts.
  // The argument `get_time` should be a callback that will be called to get the
  // current time in millisecond.
  explicit ScalableTimeoutDriver(std::function<TimeMs()> get_time)
      : get_time_(std::move(get_time)), last_tick_(get_time()) {}

  // This method should be called on a periodic timer, every `kResolution`. If
  // ticks are skipped - if `Tick` was for any reason not called for a few
  // `kResolution` time, it's not necessary to compensate as the method will
  // check how long time it was since it was last called.
  void Tick();

 private:
  friend class ScalableTimeout;

  // Internal function to schedule a timeout to be triggered after `duration`.
  void Schedule(ScalableTimeout* timeout,
                DurationMs duration,
                TimeoutID timeout_id);

  // Internal function to cancel a scheduled timeout.
  void Cancel(ScalableTimeout* timeout);

  // Internal function to reschedule an already scheduled timeout.
  void Reschedule(ScalableTimeout* timeout,
                  DurationMs duration,
                  TimeoutID timeout_id);

  // Internal function to do the schedule, while holding the mutex.
  void ScheduleLocked(ScalableTimeout* timeout,
                      TimeMs expiry,
                      TimeoutID timeout_id)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // The single mutex which is held when doing all operations on timeouts.
  webrtc::Mutex mutex_;

  // factory function to call, to get the current time.
  const std::function<TimeMs()> get_time_;

  // The time when `Tick` was last called.
  TimeMs last_tick_ RTC_GUARDED_BY(mutex_);

  // The position in `buckets_` that was used when `Tick` was last called.
  int last_tick_position_ RTC_GUARDED_BY(mutex_) = 0;

  // The actual wheel. It's a circular buffer where each entry is a
  // doubly-linked list of timeouts that will expire at the same time slot.
  std::array<ScalableTimeout*, kBucketCount> buckets_
      RTC_GUARDED_BY(mutex_) = {};
};

// The `ScalableTimeoutFactory` creates timeouts, which a socket will do when it
// calls `DcSctpSocketCallback::CreateTimeout`.
//
// It's very important to know that the `on_timeout_expired` callback will be
// called from the timer driver thread; i.e. the thread calling
// `ScalableTimeoutDriver::Tick`. As the actual socket is likely managed by
// another thread or managed by a task queue, the callback should simply post a
// message to that thread/queue, which will then call
// `DcSctpSocket::HandleTimeout` from the correct thread. It is absolutely not
// allowed to call `DcSctpSocket::HandleTimeout` from within this callback, as
// it's running on the timer driver thread.
//
// This factory object must outlive any timeouts created by it, which in essence
// means that it must outlive the DcSctpSocket it's serving, as all timeouts are
// deleted when a socket is deleted.
//
// Lastly, keep in mind that TimeoutID is not unique across sockets, so when a
// timeout expires, and the `on_timeout_expired` is invoked, you must call
// `DcSctpSocket::HandleTimeout` on the socket that created the Timeout it
// originates from. Because of that, The `ScalableTimeoutFactory` cannot be
// shared by multiple sockets. But two `ScalableTimeoutFactory` objects can, and
// should, use the same underlying `TimoutWheelDriver` object.
class ScalableTimeoutFactory {
 public:
  ScalableTimeoutFactory(
      ScalableTimeoutDriver* driver,
      std::function<void(TimeoutID timeout_id)> on_timeout_expired)
      : driver_(*driver), on_timeout_expired_(std::move(on_timeout_expired)) {}

  std::unique_ptr<Timeout> CreateTimeout();

 private:
  friend class ScalableTimeoutDriver;

  ScalableTimeoutDriver& driver_;
  const std::function<void(TimeoutID timeout_id)> on_timeout_expired_;
};

// An implementation of the Timeout interface, created by
// `ScalableTimeoutFactory`.
class ScalableTimeout : public Timeout {
 public:
  explicit ScalableTimeout(ScalableTimeoutDriver* driver,
                           ScalableTimeoutFactory* factory)
      : driver_(*driver), factory_(*factory) {}
  ~ScalableTimeout() override;

  void Start(DurationMs duration, TimeoutID timeout_id) override {
    driver_.Schedule(this, duration, timeout_id);
  }

  void Stop() override { driver_.Cancel(this); }

  void Restart(DurationMs duration, TimeoutID timeout_id) override {
    driver_.Reschedule(this, duration, timeout_id);
  }

  TimeoutID timeout_id() const { return timeout_id_; }

 private:
  friend class ScalableTimeoutDriver;

  // Inserts itself to the list starting at `list_head`. Will be called while
  // holding `driver_.mutex_`.
  void InsertAt(ScalableTimeout** list_head);
  // Removes itself from the linked list of timers that it is a member of. Will
  // be called while holding `driver_.mutex_`.
  void Unlink();

  // The ScalableTimeoutDriver that drives it.
  ScalableTimeoutDriver& driver_;

  // The factory that created it.
  ScalableTimeoutFactory& factory_;

  // All the members below are only accessed while holding `driver_.mutex_`.

  // A traditional "Asymmetric doubly linked list", where `prevs_next` points to
  // the previous element's `next_` element, for easier removal without having
  // to know the list the element is contained in.
  ScalableTimeout** prevs_next_ = nullptr;
  ScalableTimeout* next_ = nullptr;

  // The number of revolutions left that the timer must spin past this timer
  // before it has expired. Will be valid only when it's running.
  size_t revolutions_;

  // The current TimeoutID that was provided in the `Start`/`Restart` call.
  TimeoutID timeout_id_;
};

}  // namespace dcsctp
#endif  // NET_DCSCTP_TIMER_SCALABLE_TIMEOUT_H_
