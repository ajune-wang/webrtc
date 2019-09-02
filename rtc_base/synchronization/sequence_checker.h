/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_
#define RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_

#include "api/task_queue/task_queue_base.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
// Real implementation of SequenceChecker, for use in debug mode, or
// for temporary use in release mode (e.g. to RTC_CHECK on a threading issue
// seen only in the wild).
//
// Note: You should almost always use the SequenceChecker class to get the
// right version for your build configuration.
class SequenceCheckerImpl {
 public:
  SequenceCheckerImpl();
  ~SequenceCheckerImpl();

  bool IsCurrent() const;
  // Changes the task queue or thread that is checked for in IsCurrent. This can
  // be useful when an object may be created on one task queue / thread and then
  // used exclusively on another thread.
  void Detach();

 private:
  rtc::CriticalSection lock_;
  // These are mutable so that IsCurrent can set them.
  mutable bool attached_ RTC_GUARDED_BY(lock_);
  mutable rtc::PlatformThreadRef valid_thread_ RTC_GUARDED_BY(lock_);
  mutable const TaskQueueBase* valid_queue_ RTC_GUARDED_BY(lock_);
  mutable const void* valid_system_queue_ RTC_GUARDED_BY(lock_);
};

// Do nothing implementation, for use in release mode.
//
// Note: You should almost always use the SequenceChecker class to get the
// right version for your build configuration.
class SequenceCheckerDoNothing {
 public:
  bool IsCurrent() const { return true; }
  void Detach() {}
};

// SequenceChecker is a helper class used to help verify that some methods
// of a class are called on the same task queue or thread. A
// SequenceChecker is bound to a a task queue if the object is
// created on a task queue, or a thread otherwise.
//
//
// Example:
// class MyClass {
//  public:
//   void Foo() {
//     RTC_DCHECK_RUN_ON(sequence_checker_);
//     ... (do stuff) ...
//   }
//
//  private:
//   SequenceChecker sequence_checker_;
// }
//
// In Release mode, IsCurrent will always return true.
#if RTC_DCHECK_IS_ON
class RTC_LOCKABLE SequenceChecker : public SequenceCheckerImpl {};
#else
class RTC_LOCKABLE SequenceChecker : public SequenceCheckerDoNothing {};
#endif  // RTC_ENABLE_THREAD_CHECKER

namespace webrtc_seq_check_impl {
// Helper class used by RTC_DCHECK_RUN_ON (see example usage below).
class RTC_SCOPED_LOCKABLE SequenceCheckerScope {
 public:
  template <typename ThreadLikeObject>
  explicit SequenceCheckerScope(const ThreadLikeObject* thread_like_object)
      RTC_EXCLUSIVE_LOCK_FUNCTION(thread_like_object) {}
  SequenceCheckerScope(const SequenceCheckerScope&) = delete;
  SequenceCheckerScope& operator=(const SequenceCheckerScope&) = delete;
  ~SequenceCheckerScope() RTC_UNLOCK_FUNCTION() {}

  template <typename ThreadLikeObject>
  static bool IsCurrent(const ThreadLikeObject* thread_like_object) {
    return thread_like_object->IsCurrent();
  }
};
}  // namespace webrtc_seq_check_impl
}  // namespace webrtc

// RTC_RUN_ON/RTC_GUARDED_BY/RTC_DCHECK_RUN_ON macros allows to annotate
// variables are accessed from same thread/task queue.
// Using tools designed to check mutexes, it checks at compile time everywhere
// variable is access, there is a run-time dcheck thread/task queue is correct.
//
// class ThreadExample {
//  public:
//   void NeedVar1() {
//     RTC_DCHECK_RUN_ON(network_thread_);
//     transport_->Send();
//   }
//
//  private:
//   rtc::Thread* network_thread_;
//   int transport_ RTC_GUARDED_BY(network_thread_);
// };
//
// class SequenceCheckerExample {
//  public:
//   int CalledFromPacer() RTC_RUN_ON(pacer_sequence_checker_) {
//     return var2_;
//   }
//
//   void CallMeFromPacer() {
//     RTC_DCHECK_RUN_ON(&pacer_sequence_checker_)
//        << "Should be called from pacer";
//     CalledFromPacer();
//   }
//
//  private:
//   int pacer_var_ RTC_GUARDED_BY(pacer_sequence_checker_);
//   SequenceChecker pacer_sequence_checker_;
// };
//
// class TaskQueueExample {
//  public:
//   class Encoder {
//    public:
//     rtc::TaskQueue* Queue() { return encoder_queue_; }
//     void Encode() {
//       RTC_DCHECK_RUN_ON(encoder_queue_);
//       DoSomething(var_);
//     }
//
//    private:
//     rtc::TaskQueue* const encoder_queue_;
//     Frame var_ RTC_GUARDED_BY(encoder_queue_);
//   };
//
//   void Encode() {
//     // Will fail at runtime when DCHECK is enabled:
//     // encoder_->Encode();
//     // Will work:
//     rtc::scoped_refptr<Encoder> encoder = encoder_;
//     encoder_->Queue()->PostTask([encoder] { encoder->Encode(); });
//   }
//
//  private:
//   rtc::scoped_refptr<Encoder> encoder_;
// }

// Document if a function expected to be called from same thread/task queue.
// TODO(tommi): The name of this macro conflicts with RTC_[D]CHECK_RUN_ON
// macros which are used within a scope - while this one is an annotation
// macro.
#define RTC_RUN_ON(x) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(x))

#if RTC_DCHECK_IS_ON
#define RTC_SEQUENCE_CHECKER(name) webrtc::SequenceChecker name

#define RTC_DCHECK_IS_CURRENT(name) RTC_DCHECK((name).IsCurrent())

#define RTC_DCHECK_RUN_ON(x)                                              \
  webrtc::webrtc_seq_check_impl::SequenceCheckerScope seq_check_scope(x); \
  RTC_DCHECK((x)->IsCurrent())

// TODO(tommi): The name "CHECK" implies that there's a runtime CHECK() that
// will halt the process in a non-DCHECK release build. However, this macro
// is more about doing DCHECKs in DCHECK enabled builds and still supporting
// pseudo 'locking' for TaskQueue/Thread objects in release builds without
// code size impact. (kind of like the RTC_DCHECK_RUN_ON macro actually... hmmm)
#define RTC_CHECK_RUN_ON(x) RTC_DCHECK_RUN_ON(x)

// Used for function declarations.
#define RTC_DECLARE_RUN_ON(x) RTC_RUN_ON(x)

#define RTC_DETACH_FROM_SEQUENCE(name) (name).Detach()
#define RTC_GUARDED_BY_SEQUENCE(name) RTC_GUARDED_BY(name)
#define RTC_PT_GUARDED_BY_SEQUENCE(name) RTC_PT_GUARDED_BY(name)
#else  // RTC_DCHECK_IS_ON
#define RTC_SEQUENCE_CHECKER(name) static_assert(true, "")
#define RTC_DCHECK_IS_CURRENT(name) RTC_EAT_STREAM_PARAMETERS(0)
#define RTC_DCHECK_RUN_ON(x) RTC_EAT_STREAM_PARAMETERS(0)
#define RTC_CHECK_RUN_ON(x) \
  webrtc::webrtc_seq_check_impl::SequenceCheckerScope seq_check_scope(x);

#define RTC_DECLARE_RUN_ON(x)

#define RTC_DETACH_FROM_SEQUENCE(name)
#define RTC_GUARDED_BY_SEQUENCE(name)
#define RTC_PT_GUARDED_BY_SEQUENCE(name)
#endif  // RTC_DCHECK_IS_ON

#endif  // RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_
