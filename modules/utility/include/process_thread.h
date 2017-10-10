/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_
#define MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_

#include <list>
#include <memory>
#include <queue>

#include "rtc_base/criticalsection.h"
#include "rtc_base/location.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/event_wrapper.h"
#include "typedefs.h"  // NOLINT(build/include)

#if defined(WEBRTC_WIN)
// Due to a bug in the std::unique_ptr implementation that ships with MSVS,
// we need the full definition of QueuedTask, on Windows.
#include "rtc_base/task_queue.h"
#else
namespace rtc {
class QueuedTask;
}
#endif

namespace webrtc {
class Module;

class ProcessThread {
 public:
  static std::unique_ptr<ProcessThread> Create(const char* thread_name);

  ProcessThread(const char* thread_name);
  ~ProcessThread();

  // Starts the worker thread.  Must be called from the construction thread.
  void Start();

  // Stops the worker thread.  Must be called from the construction thread.
  void Stop();

  // Wakes the thread up to give a module a chance to do processing right
  // away.  This causes the worker thread to wake up and requery the specified
  // module for when it should be called back. (Typically the module should
  // return 0 from TimeUntilNextProcess on the worker thread at that point).
  // Can be called on any thread.
  void WakeUp(Module* module);

  // Queues a task object to run on the worker thread.  Ownership of the
  // task object is transferred to the ProcessThread and the object will
  // either be deleted after running on the worker thread, or on the
  // construction thread of the ProcessThread instance, if the task did not
  // get a chance to run (e.g. posting the task while shutting down or when
  // the thread never runs).
  void PostTask(std::unique_ptr<rtc::QueuedTask> task);

  // Adds a module that will start to receive callbacks on the worker thread.
  // Can be called from any thread.
  void RegisterModule(Module* module, const rtc::Location& from);

  // Removes a previously registered module.
  // Can be called from any thread.
  void DeRegisterModule(Module* module);

 private:
  struct ModuleCallback {
    ModuleCallback() = delete;
    ModuleCallback(ModuleCallback&& cb) = default;
    ModuleCallback(const ModuleCallback& cb) = default;
    ModuleCallback(Module* module, const rtc::Location& location)
        : module(module), location(location) {}
    bool operator==(const ModuleCallback& cb) const {
      return cb.module == module;
    }

    Module* const module;
    int64_t next_callback = 0;  // Absolute timestamp.
    const rtc::Location location;

   private:
    ModuleCallback& operator=(ModuleCallback&);
  };

  typedef std::list<ModuleCallback> ModuleList;

  static bool Run(void* obj);
  bool Process();
  // Warning: For some reason, if |lock_| comes immediately before |modules_|
  // with the current class layout, we will  start to have mysterious crashes
  // on Mac 10.9 debug.  I (Tommi) suspect we're hitting some obscure alignemnt
  // issues, but I haven't figured out what they are, if there are alignment
  // requirements for mutexes on Mac or if there's something else to it.
  // So be careful with changing the layout.
  rtc::CriticalSection lock_;  // Used to guard modules_, tasks_ and stop_.

  rtc::ThreadChecker thread_checker_;
  const std::unique_ptr<EventWrapper> wake_up_;
  // TODO(pbos): Remove unique_ptr and stop recreating the thread.
  std::unique_ptr<rtc::PlatformThread> thread_;

  ModuleList modules_;
  std::queue<rtc::QueuedTask*> queue_;
  bool stop_;
  const char* thread_name_;
};

}  // namespace webrtc

#endif // MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_
