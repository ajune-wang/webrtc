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
#include <memory>
#include <set>

#include "api/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
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
  ~Receiver() { RTC_DCHECK(ref_count_ == 0); }

 private:
  int ref_count_;
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
  // Connecting on construction is generally safer since it ensures proper
  // destruction order. However it is not always practical to do, therefore
  // an empty constructor is provided as well.
  Junction() { is_active_.store(false); }
  explicit Junction(Receiver<MSG_T>* receiver) : Junction() {
    Connect(receiver);
  }
  void OnMessage(MSG_T msg) final {
    is_active_.store(true);
    RTC_DCHECK(receivers_.size() >= 1);
    for (auto& receiver : receivers_)
      receiver->OnMessage(msg);
  }
  void Connect(Receiver<MSG_T>* receiver) final {
    RTC_DCHECK(!is_active_);
    receiver->ref_count_++;
    receivers_.insert(receiver);
  }
  // Note that using proper destruction order is preffered to explicit
  // disconnection
  void Disconnect(Receiver<MSG_T>* receiver) final {
    receivers_.erase(receiver);
    receiver->ref_count_--;
  }
  virtual ~Junction() {
    for (auto& receiver : receivers_)
      receiver->ref_count_--;
    receivers_.clear();
  }

 private:
  std::atomic<bool> is_active_;
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

// The TaskQueueReceiver calls the handler function on the given task queue. It
// also protects against destruction while running. It tries to stop any pending
// tasks and waits until any running tasks are done. Any resources destructed
// after the TaskQueueReceiver is protected from access upon destuction.
template <typename MSG_T>
class TaskQueueReceiver : public HandlingReceiver<MSG_T> {
 public:
  using handler_t = std::function<void(MSG_T)>;
  explicit TaskQueueReceiver(rtc::TaskQueue* target_queue)
      : target_queue_(target_queue) {
    task_count_.store(0);
  }
  void SetHandler(handler_t handler) override {
    RTC_DCHECK(!handler_);
    handler_ = handler;
  }
  virtual ~TaskQueueReceiver() { StopAndWait(); }

 protected:
  void OnMessage(MSG_T msg) override {
    RTC_DCHECK(handler_);
    RTC_DCHECK(target_queue_);
    if (task_count_++ != kReceiverStopped) {
      target_queue_->PostTask(
          std::bind(&TaskQueueReceiver<MSG_T>::HandleMessage, this, msg));
    }
  }

 private:
  void HandleMessage(MSG_T msg) {
    handler_(msg);
    task_count_--;
  }
  void StopAndWait() {
    int empty_count = 0;
    while (
        !task_count_.compare_exchange_strong(empty_count, kReceiverStopped)) {
      if (empty_count == kReceiverStopped)
        return;
      rtc::Event event(false, false);
      target_queue_->PostTask([&event]() { event.Set(); });
      event.Wait(rtc::Event::kForever);
      empty_count = 0;
    }
  }
  static constexpr int kReceiverStopped = -1;
  std::atomic_int task_count_;
  handler_t handler_;
  rtc::TaskQueue* target_queue_;
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
