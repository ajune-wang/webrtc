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

class InvokeResult {
 public:
  using uptr = std::unique_ptr<InvokeResult>;
  virtual ~InvokeResult();
};

// Wraps a taskqueue and makes sure pending tasks are not run after handler has
// been destructed.
class QueueTaskRunner {
 private:
  struct HandlerState {
    bool alive = true;
  };

 public:
  using closure_t = std::function<void()>;
  using AliveToken = rtc::RefCountedObject<HandlerState>;
  explicit QueueTaskRunner(rtc::TaskQueue* target_queue);
  ~QueueTaskRunner();
  void PostTask(closure_t&& closure);
  InvokeResult::uptr InvokeTask(closure_t&& closure);

  void StopTasks();
  void PostDelayedTask(closure_t&& closure, TimeDelta delay);

 private:
  rtc::TaskQueue* target_queue_;
  rtc::scoped_refptr<AliveToken> token_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(QueueTaskRunner);
};

namespace internal {
class InvokeResults : public InvokeResult {
 public:
  using uptr = std::unique_ptr<InvokeResults>;
  explicit InvokeResults(int reserved);
  ~InvokeResults() final;
  void AddResult(InvokeResult::uptr&& result);

 private:
  std::vector<InvokeResult::uptr> inner_results_;
};
}  // namespace internal

// Implementing all the templated functionality within the Message struct makes
// it easy to provide template instantiations for all types that are used as
// messages.
template <typename MSG_T>
struct SignalMessage {
 private:
  class Broadcaster;

 public:
  class Producer;
  class ProducerConnector;
  class ReceiverConnector;
  // A receiver is an endpoint for messages. It can only be called from a
  // producer which always will increase the reference count before being able
  // to call a receiver. This provides some thread safety in destruction, since
  // the ref count is DCHECKED in all receiver destructors. It is, however up to
  // the implementor to make sure that a receiver is destructed before any
  // resources it might access.

  class Receiver {
   public:
    Receiver() : ref_count_(0) {}

    void ReceiveFrom(const ProducerConnector& prod) {
      prod.producer_->Connect(this);
    }
    void EndReceiveFrom(const ProducerConnector& prod) {
      prod.producer_->Disconnect(this);
    }

    void AssignReceiverTo(ReceiverConnector* recv) { recv->receiver_ = this; }

   protected:
    ~Receiver() { RTC_DCHECK(ref_count_.load() == 0); }

    friend class Broadcaster;
    virtual void OnMessage(MSG_T msg) = 0;
    virtual InvokeResult::uptr BeginInvoke(MSG_T msg) {
      OnMessage(msg);
      return rtc::MakeUnique<InvokeResult>();
    }

   private:
    std::atomic<int> ref_count_;
    RTC_DISALLOW_COPY_AND_ASSIGN(Receiver);
  };

  // The observer interface is useful where it's safe to directly call an
  // object. It implements the receiver interface to be possible to use with
  // Junctions.

  class Observer : public virtual Receiver {
   public:
    Observer() {}
    void OnMessage(MSG_T msg) override = 0;
    InvokeResult::uptr BeginInvoke(MSG_T msg) override = 0;

   protected:
    ~Observer() {}

    RTC_DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // A producer is the sending endpoint of a message. The actual sending is not
  // handled in the producer interface. The producer interface is only there to
  // allow connections to be made.

  class Producer {
   public:
    Producer() {}
    virtual void Connect(Receiver* observer) = 0;
    virtual void Disconnect(Receiver* observer) = 0;

    void Connect(const ReceiverConnector& conn) { Connect(conn.receiver_); }
    void Disconnect(const ReceiverConnector& conn) {
      Disconnect(conn.receiver_);
    }

    void AssignProducerTo(ProducerConnector* prod) { prod->producer_ = this; }

   protected:
    ~Producer() {}
    RTC_DISALLOW_COPY_AND_ASSIGN(Producer);
  };

  class ProducerConnector {
   private:
    friend class Producer;
    friend class Receiver;
    Producer* producer_ = nullptr;
  };

  class ReceiverConnector {
   private:
    friend class Producer;
    friend class Receiver;
    Receiver* receiver_ = nullptr;
  };

 private:
  class Broadcaster {
   public:
    Broadcaster() {}
    ~Broadcaster() { RTC_DCHECK(receivers_.size() == 0); }

    void Connect(Receiver* receiver) {
      ++receiver->ref_count_;
      receivers_.insert(receiver);
    }
    void Disconnect(Receiver* receiver) {
      receivers_.erase(receiver);
      --receiver->ref_count_;
    }
    void OnMessage(MSG_T msg) {
      RTC_DCHECK(receivers_.size() >= 1);
      for (auto& receiver : receivers_)
        receiver->OnMessage(msg);
    }
    InvokeResult::uptr BeginInvoke(MSG_T msg) {
      RTC_DCHECK(receivers_.size() >= 1);
      internal::InvokeResults::uptr results =
          rtc::MakeUnique<internal::InvokeResults>(receivers_.size());
      for (auto& receiver : receivers_)
        results->AddResult(receiver->BeginInvoke(msg));
      // Destructor of results guarantees that invoked tasks are finished
      return results;
    }

   private:
    std::set<Receiver*> receivers_;
    RTC_DISALLOW_COPY_AND_ASSIGN(Broadcaster);
  };

 public:
  // The Junction is the default way to send a message, it will send the given
  // message to any connected receivers. It keeps a reference to any connected
  // receivers, this allows the receivers to check for invalid destruction
  // order. Note that it will not protect against being destructed while being
  // used itself. The destructor should only be run when there's no possible
  // users.
  class Junction : public virtual Observer, public virtual Producer {
   public:
    using uptr = std::unique_ptr<Junction>;
    Junction() {}
    virtual ~Junction() {}
  };

  class SimpleJunction : public Junction {
   public:
    SimpleJunction() {}
    ~SimpleJunction() override{};
    void OnMessage(MSG_T msg) final { broadcaster_.OnMessage(msg); }
    InvokeResult::uptr BeginInvoke(MSG_T msg) final {
      return broadcaster_.BeginInvoke(msg);
    }
    void Connect(Receiver* receiver) final { broadcaster_.Connect(receiver); }
    void Disconnect(Receiver* receiver) final {
      broadcaster_.Disconnect(receiver);
    }

   private:
    Broadcaster broadcaster_;
    RTC_DISALLOW_COPY_AND_ASSIGN(SimpleJunction);
  };

  // The TaskQueueJunction calls the receivers on the given task queue. It also
  // protects against destruction while running. It tries to stop any pending
  // tasks and waits until any running tasks are done. Any resources destructed
  // after the TaskQueueJunction is protected from access upon destuction of the
  // TaskQueueJunction.

  class TaskQueueJunction : public Junction {
   public:
    explicit TaskQueueJunction(QueueTaskRunner* target_queue)
        : pending_tasks_(target_queue) {}
    virtual ~TaskQueueJunction() { pending_tasks_->StopTasks(); }
    void Connect(Receiver* receiver) override {
      broadcaster_.Connect(receiver);
    }
    void Disconnect(Receiver* receiver) override {
      broadcaster_.Disconnect(receiver);
    }
    void OnMessage(MSG_T msg) override {
      pending_tasks_->PostTask(
          std::bind(&Broadcaster::OnMessage, &broadcaster_, msg));
    }
    InvokeResult::uptr BeginInvoke(MSG_T msg) override {
      return pending_tasks_->InvokeTask(
          std::bind(&Broadcaster::BeginInvoke, &broadcaster_, msg));
    }
    void DelayMessage(MSG_T msg, TimeDelta delay) {
      pending_tasks_->PostDelayedTask(
          std::bind(&Broadcaster::OnMessage, &broadcaster_, msg), delay);
    }

   private:
    QueueTaskRunner* pending_tasks_;
    Broadcaster broadcaster_;
    RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(TaskQueueJunction);
  };

  // The locked junction protects the interface with an externally provided
  // critical section. This is mostly useful for debugging since it creates deep
  // call stacks rather than the shallow handler call stacks provided by the
  // task queue based junction. Note however that it will block on all calls and
  // therefore can be costly in performance and might risk deadlocking.

  class LockedJunction : public Junction {
   public:
    using handler_t = std::function<void(MSG_T)>;
    explicit LockedJunction(rtc::CriticalSection* lock)
        : borrowed_lock_(lock) {}
    virtual ~LockedJunction() = default;
    void Connect(Receiver* receiver) override {
      rtc::CritScope cs(borrowed_lock_);
      broadcaster_.Connect(receiver);
    }
    void Disconnect(Receiver* receiver) override {
      rtc::CritScope cs(borrowed_lock_);
      broadcaster_.Disconnect(receiver);
    }
    void OnMessage(MSG_T msg) override {
      rtc::CritScope cs(borrowed_lock_);
      broadcaster_.OnMessage(msg);
    }
    InvokeResult::uptr BeginInvoke(MSG_T msg) final {
      rtc::CritScope cs(borrowed_lock_);
      return broadcaster_.BeginInvoke(msg);
    }

   private:
    rtc::CriticalSection* borrowed_lock_;
    Broadcaster broadcaster_;
    RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(LockedJunction);
  };

  class Terminator : public Receiver {
   public:
    Terminator() = default;
    virtual ~Terminator() = default;
    void OnMessage(MSG_T msg) override {}
    RTC_DISALLOW_COPY_AND_ASSIGN(Terminator);
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

    void OnMessage(MSG_T msg) override {
      rtc::CritScope cs(&lock_);
      last_message_ = msg;
    }

   private:
    rtc::Optional<MSG_T> last_message_;
    rtc::CriticalSection lock_;
    RTC_DISALLOW_COPY_AND_ASSIGN(CacheReceiver);
  };

  // The HandlingReceiver allows setting the handling function of a receiver.
  class MessageHandler : public Receiver {
   public:
    using handler_t = std::function<void(MSG_T)>;
    MessageHandler() = default;
    virtual ~MessageHandler() = default;
    void SetHandler(handler_t handler) {
      RTC_DCHECK(!handler_);
      handler_ = handler;
    }
    template <class CLASS_T>
    void Bind(CLASS_T* handler_class, void (CLASS_T::*method)(MSG_T)) {
      SetHandler(std::bind(method, handler_class, std::placeholders::_1));
    }
    void OnMessage(MSG_T msg) override {
      RTC_DCHECK(handler_);
      if (!handler_)
        return;
      handler_(msg);
    }

   private:
    handler_t handler_;
    RTC_DISALLOW_COPY_AND_ASSIGN(MessageHandler);
  };
};
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_MESSAGE_H_
