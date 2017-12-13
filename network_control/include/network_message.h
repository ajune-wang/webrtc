/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_CONTROL_INCLUDE_NETWORK_MESSAGE_H_
#define NETWORK_CONTROL_INCLUDE_NETWORK_MESSAGE_H_

#include <atomic>
#include <functional>
#include <initializer_list>
#include <memory>
#include <set>

#include "api/optional.h"
#include "network_control/include/network_units.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
namespace network {
namespace signal {

template <typename MSG_T>
class Junction;

// Receiver only allows calls from a Junction, that way it can flag if it is
// destructed in a situation where it could possibly be called.
template <typename MSG_T>
class Receiver {
 public:
  Receiver() : ref_count_(0) {}

 protected:
  friend class Junction<MSG_T>;
  virtual void OnMessage(MSG_T msg) = 0;
  virtual void DelayMessage(MSG_T msg, units::TimeDelta delay) {
    RTC_NOTREACHED() << "DelayMessage only works on TaskQueue receivers. ";
  }
  ~Receiver() { RTC_DCHECK(ref_count_.load() == 0); }

 private:
  std::atomic<int> ref_count_;
  RTC_DISALLOW_COPY_AND_ASSIGN(Receiver);
};

template <typename MSG_T>
class Producer {
 public:
  Producer() {}
  virtual void Connect(Receiver<MSG_T>* observer) = 0;
  virtual void Disconnect(Receiver<MSG_T>* observer) = 0;

 protected:
  ~Producer() {}
  RTC_DISALLOW_COPY_AND_ASSIGN(Producer);
};

// An Observer can be used as a Receiver by Junctions or be called directly.
template <typename MSG_T>
class Observer : public Receiver<MSG_T> {
 public:
  Observer() {}
  void OnMessage(MSG_T msg) override = 0;

 protected:
  ~Observer() {}

  RTC_DISALLOW_COPY_AND_ASSIGN(Observer);
};

// The Junction is the default way to send a message, it will send the given
// message to any connected receivers. It keeps a reference to any connected
// receivers, this allows the receivers to check for invalid destruction order.
// Note that it will not protect against being destructed while being used
// itself. The destructor should only be run when there's no possible users.
template <typename MSG_T>
class Junction : public Observer<MSG_T>, public Producer<MSG_T> {
 public:
  Junction() {}

  void OnMessage(MSG_T msg) final {
    RTC_DCHECK(receivers_.size() >= 1);
    for (auto& receiver : receivers_)
      receiver->OnMessage(msg);
  }
  void DelayMessage(MSG_T msg, units::TimeDelta delay) final {
    RTC_DCHECK(receivers_.size() >= 1);
    for (auto& receiver : receivers_)
      receiver->DelayMessage(msg, delay);
  }
  void Connect(Receiver<MSG_T>* receiver) final {
    ++receiver->ref_count_;
    receivers_.insert(receiver);
  }
  void Disconnect(Receiver<MSG_T>* receiver) final {
    receivers_.erase(receiver);
    --receiver->ref_count_;
  }
  virtual ~Junction() { RTC_DCHECK(receivers_.size() == 0); }

 private:
  std::set<Receiver<MSG_T>*> receivers_;
  RTC_DISALLOW_COPY_AND_ASSIGN(Junction);
};
template <typename MSG_T>
class Handler {
 public:
  using handler_t = std::function<void(MSG_T)>;
  Handler() = default;
  virtual void SetHandler(handler_t handler) = 0;

 protected:
  ~Handler() = default;
};

template <typename MSG_T>
class HandlingReceiver : public Receiver<MSG_T>, public Handler<MSG_T> {
 public:
  using uptr = std::unique_ptr<HandlingReceiver>;
  HandlingReceiver() = default;
  virtual ~HandlingReceiver() = default;
};

// The SameThreadReceiver calls the handler function directly. This should
// generally be avoided since it will block until the handler is finished and
// will have to make sure that any required locks are taken to protect resources
// shared between threads. TaskQueueReceiver is the preffered receiver.
template <typename MSG_T>
class SameThreadReceiver : public HandlingReceiver<MSG_T> {
 public:
  using handler_t = std::function<void(MSG_T)>;
  SameThreadReceiver() = default;
  void SetHandler(handler_t handler) override {
    if (msg_.has_value()) {
      handler(*msg_);
      msg_.reset();
    }
    handler_ = handler;
  }
  virtual ~SameThreadReceiver() = default;

 protected:
  void OnMessage(MSG_T msg) override {
    if (!handler_)
      msg_ = msg;
    else
      handler_(msg);
  }

 private:
  rtc::Optional<MSG_T> msg_;
  handler_t handler_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SameThreadReceiver);
};

template <typename MSG_T>
class LockedReceiver : public HandlingReceiver<MSG_T> {
 public:
  using handler_t = std::function<void(MSG_T)>;
  explicit LockedReceiver(rtc::CriticalSection* lock) : borrowed_lock_(lock) {}
  void SetHandler(handler_t handler) override {
    rtc::CritScope cs(borrowed_lock_);
    handler_ = handler;
  }
  virtual ~LockedReceiver() = default;

 protected:
  void OnMessage(MSG_T msg) override {
    RTC_DCHECK(handler_);
    if (!handler_)
      return;
    rtc::CritScope cs(borrowed_lock_);
    handler_(msg);
  }

 private:
  rtc::CriticalSection* borrowed_lock_;
  handler_t handler_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(LockedReceiver);
};

namespace internal {
// Wraps a taskqueue and makes sure pending tasks are not run after handler has
// been destructed.
class PendingTaskHandler {
 private:
  using closure_t = std::function<void()>;
  struct HandlerState {
    bool alive = true;
  };
  using AliveToken = rtc::RefCountedObject<HandlerState>;
  class PendingTask : public rtc::QueuedTask {
   public:
    PendingTask(closure_t&& closure, rtc::scoped_refptr<AliveToken> token);
    ~PendingTask() final;
    bool Run() final;
    void Cancel();

   private:
    std::function<void()> closure_;
    rtc::scoped_refptr<AliveToken> token_;
    RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PendingTask);
  };

 public:
  explicit PendingTaskHandler(rtc::TaskQueue* target_queue);
  ~PendingTaskHandler();
  void PostTask(closure_t&& closure);
  void PostDelayedTask(closure_t&& closure, units::TimeDelta delay);

 private:
  rtc::TaskQueue* target_queue_;
  rtc::scoped_refptr<AliveToken> token_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PendingTaskHandler);
};
}  // namespace internal

// The TaskQueueReceiver calls the handler function on the given task queue. It
// also protects against destruction while running. It tries to stop any pending
// tasks and waits until any running tasks are done. Any resources destructed
// after the TaskQueueReceiver is protected from access upon destuction.
template <typename MSG_T>
class TaskQueueReceiver : public HandlingReceiver<MSG_T> {
 public:
  using handler_t = std::function<void(MSG_T)>;
  explicit TaskQueueReceiver(rtc::TaskQueue* target_queue)
      : pending_tasks_(target_queue) {}

  void SetHandler(handler_t handler) override {
    RTC_DCHECK(!handler_);
    handler_ = handler;
  }
  virtual ~TaskQueueReceiver() {}

 protected:
  void OnMessage(MSG_T msg) override {
    RTC_DCHECK(handler_);
    pending_tasks_.PostTask(std::bind(handler_, msg));
  }
  void DelayMessage(MSG_T msg, units::TimeDelta delay) override {
    RTC_DCHECK(handler_);
    pending_tasks_.PostDelayedTask(std::bind(handler_, msg), delay);
  }

 private:
  handler_t handler_;
  internal::PendingTaskHandler pending_tasks_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(TaskQueueReceiver);
};

template <typename MSG_T>
struct Message {
  using Receiver = signal::Receiver<MSG_T>;
  using Producer = signal::Producer<MSG_T>;
  using Observer = signal::Observer<MSG_T>;
  using Junction = signal::Junction<MSG_T>;
  using Handler = signal::Handler<MSG_T>;
  using HandlingReceiver = signal::HandlingReceiver<MSG_T>;
  using SimpleReceiver = signal::SameThreadReceiver<MSG_T>;
  using LockedReceiver = signal::LockedReceiver<MSG_T>;
  using TaskQueueReceiver = signal::TaskQueueReceiver<MSG_T>;
};
}  // namespace signal
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_MESSAGE_H_
