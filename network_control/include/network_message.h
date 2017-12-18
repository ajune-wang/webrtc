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
#include <vector>

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

namespace internal {
class InvokeResult {
 public:
  using uptr = std::unique_ptr<InvokeResult>;
  virtual ~InvokeResult() {}
};
// Wraps a taskqueue and makes sure pending tasks are not run after handler has
// been destructed.
class PendingTaskHandler {
  using closure_t = std::function<void()>;

 private:
  struct HandlerState {
    bool alive = true;
  };

 public:
  using AliveToken = rtc::RefCountedObject<HandlerState>;
  explicit PendingTaskHandler(rtc::TaskQueue* target_queue);
  ~PendingTaskHandler();
  internal::InvokeResult::uptr InvokeTask(closure_t&& closure);
  void PostTask(closure_t&& closure);
  void PostDelayedTask(closure_t&& closure, units::TimeDelta delay);

 private:
  rtc::TaskQueue* target_queue_;
  rtc::scoped_refptr<AliveToken> token_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PendingTaskHandler);
};
}  // namespace internal

// Implementing all the templated functionality within the Message struct makes
// it easy to provide template instantiations for all types that are used as
// messages.
template <typename MSG_T>
struct Message {
  using handler_t = std::function<void(MSG_T)>;
  class Junction;

  // A receiver is an endpoint for messages. It can only be called from a
  // producer which always will increase the reference cound before being eble
  // to call a receiver. This provides some thread safety in destruction, since
  // the ref cound is DCHECKED in all receiver destructors. It is, however up to
  // the implementor to make sure that a receiver is destructed before any
  // resources it might access.

  class Receiver {
   public:
    Receiver() : ref_count_(0) {}

   protected:
    ~Receiver() { RTC_DCHECK(ref_count_.load() == 0); }

    friend class Junction;
    virtual void OnMessage(MSG_T msg) = 0;

    virtual internal::InvokeResult::uptr BeginInvoke(MSG_T msg) {
      OnMessage(msg);
      return rtc::MakeUnique<internal::InvokeResult>();
    }
    virtual void DelayMessage(MSG_T msg, units::TimeDelta delay) {
      RTC_NOTREACHED() << "DelayMessage only works on TaskQueue receivers. ";
    }

   private:
    std::atomic<int> ref_count_;
    RTC_DISALLOW_COPY_AND_ASSIGN(Receiver);
  };

  // A producer is the sending enpoint of a message. The actual sending is not
  // handled in the producer interface. The producer interface is only there to
  // allow connections to be made.

  class Producer {
   public:
    Producer() {}
    virtual void Connect(Receiver* observer) = 0;
    virtual void Disconnect(Receiver* observer) = 0;

   protected:
    ~Producer() {}
    RTC_DISALLOW_COPY_AND_ASSIGN(Producer);
  };

  // The observer interface is useful where it's safe to directly call an
  // object. It implements the receiver interface to be possible to use with
  // Junctions.

  class Observer : public Receiver {
   public:
    Observer() {}
    void OnMessage(MSG_T msg) override = 0;

   protected:
    ~Observer() {}

    RTC_DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // The Junction is the default way to send a message, it will send the given
  // message to any connected receivers. It keeps a reference to any connected
  // receivers, this allows the receivers to check for invalid destruction
  // order. Note that it will not protect against being destructed while being
  // used itself. The destructor should only be run when there's no possible
  // users.

  class Junction : public Observer, public Producer {
   public:
    Junction() {}

    void OnMessage(MSG_T msg) final {
      RTC_DCHECK(receivers_.size() >= 1);
      for (auto& receiver : receivers_)
        receiver->OnMessage(msg);
    }
    void Connect(Receiver* receiver) final {
      ++receiver->ref_count_;
      receivers_.insert(receiver);
    }
    void Disconnect(Receiver* receiver) final {
      receivers_.erase(receiver);
      --receiver->ref_count_;
    }

    void Invoke(MSG_T msg) {
      RTC_DCHECK(receivers_.size() >= 1);
      std::vector<internal::InvokeResult::uptr> results;
      results.reserve(receivers_.size());
      for (auto& receiver : receivers_)
        results.push_back(receiver->BeginInvoke(msg));
      // Destructor of results vector guarantees that invoked tasks are finished
    }
    void DelayMessage(MSG_T msg, units::TimeDelta delay) {
      RTC_DCHECK(receivers_.size() >= 1);
      for (auto& receiver : receivers_)
        receiver->DelayMessage(msg, delay);
    }

    void Disconnect() {
      for (auto& receiver : receivers_)
        --receiver->ref_count_;
      receivers_.clear();
    }

    virtual ~Junction() { RTC_DCHECK(receivers_.size() == 0); }

   protected:
    internal::InvokeResult::uptr BeginInvoke(MSG_T msg) final {
      Invoke(msg);
      return rtc::MakeUnique<internal::InvokeResult>();
    }

   private:
    std::set<Receiver*> receivers_;
    RTC_DISALLOW_COPY_AND_ASSIGN(Junction);
  };

  // The CacheReceiver does not allow to set handler, rather it updates the
  // intenal cache with the last received value which can be accesed from other
  // threads safely.

  class CacheReceiver : public Receiver {
   public:
    CacheReceiver() {}
    rtc::Optional<MSG_T> GetLastMessage() const {
      rtc::CritScope cs(&lock_);
      return last_message_;
    }
    virtual ~CacheReceiver() = default;

   protected:
    void OnMessage(MSG_T msg) override {
      rtc::CritScope cs(&lock_);
      last_message_ = msg;
    }

   private:
    rtc::Optional<MSG_T> last_message_;
    rtc::CriticalSection lock_;
    RTC_DISALLOW_COPY_AND_ASSIGN(CacheReceiver);
  };

  // The handler interface exposes the ability to set tha handling function of a
  // receiver without allowing additional connections to be made.

  class Handler {
   public:
    using handler_t = std::function<void(MSG_T)>;
    Handler() = default;
    virtual void SetHandler(handler_t handler) = 0;

   protected:
    ~Handler() = default;
  };

  // A handling receiver is a receiver that allows for customizing the handling
  // function.

  class HandlingReceiver : public Receiver, public Handler {
   public:
    using uptr = std::unique_ptr<HandlingReceiver>;
    HandlingReceiver() = default;
    virtual ~HandlingReceiver() = default;
  };

  // The UnsafeReceiver calls the handler function directly without taking any
  // locks. This should generally be avoided since it will block until the
  // handler is finished and the caller will have to make sure that any required
  // locks are taken to protect resources shared between threads.

  class UnsafeReceiver : public HandlingReceiver {
   public:
    using handler_t = std::function<void(MSG_T)>;
    UnsafeReceiver() = default;
    void SetHandler(handler_t handler) override {
      if (msg_.has_value()) {
        handler(*msg_);
        msg_.reset();
      }
      handler_ = handler;
    }
    virtual ~UnsafeReceiver() = default;

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
    RTC_DISALLOW_COPY_AND_ASSIGN(UnsafeReceiver);
  };

  // The locked receiver protects the interface with an externally provided
  // critical section. This is mostly useful for debugging since it creates deep
  // call stacks rather than the shallow handler call stacks provided by the
  // task queue based receiver. Note however that it will block on all calls and
  // therefore can be costly in performance and might risk deadlocking.

  class LockedReceiver : public HandlingReceiver {
   public:
    using handler_t = std::function<void(MSG_T)>;
    explicit LockedReceiver(rtc::CriticalSection* lock)
        : borrowed_lock_(lock) {}
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

  // The TaskQueueReceiver calls the handler function on the given task queue.
  // It also protects against destruction while running. It tries to stop any
  // pending tasks and waits until any running tasks are done. Any resources
  // destructed after the TaskQueueReceiver is protected from access upon
  // destuction.

  class TaskQueueReceiver : public HandlingReceiver {
   public:
    explicit TaskQueueReceiver(rtc::TaskQueue* target_queue)
        : pending_tasks_(target_queue) {}

    void SetHandler(handler_t handler) override {
      RTC_DCHECK(!handler_);
      handler_ = handler;
    }
    virtual ~TaskQueueReceiver() {}

   protected:
    internal::InvokeResult::uptr BeginInvoke(MSG_T msg) override {
      RTC_DCHECK(handler_);
      return pending_tasks_.InvokeTask(std::bind(handler_, msg));
    }
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

  static std::unique_ptr<HandlingReceiver> CreateReceiver(
      rtc::TaskQueue* optional_queue,
      rtc::CriticalSection* alternative_lock) {
    if (optional_queue != nullptr)
      return rtc::MakeUnique<TaskQueueReceiver>(optional_queue);
    else if (alternative_lock != nullptr)
      return rtc::MakeUnique<LockedReceiver>(alternative_lock);
    else
      return rtc::MakeUnique<UnsafeReceiver>();
  }
};
}  // namespace signal
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_MESSAGE_H_
