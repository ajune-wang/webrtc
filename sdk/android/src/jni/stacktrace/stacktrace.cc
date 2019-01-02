/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/stacktrace/stacktrace.h"

#include <dlfcn.h>
#include <errno.h>
#include <linux/futex.h>
#include <syscall.h>
#include <unwind.h>

#include "rtc_base/atomicops.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace jni {

namespace {

// Maximum stack trace depth we allow before aborting.
static const size_t kMaxStackSize = 100;

// This is a replacement of rtc::Event that is async-safe and doesn't use
// pthread api. This is necessary since signal handlers cannot allocate memory
// or use pthread api.
// This class is ported from Chrome.
class AsyncSafeWaitableEvent {
 public:
  AsyncSafeWaitableEvent() { rtc::AtomicOps::ReleaseStore(&futex_, 0); }
  ~AsyncSafeWaitableEvent() {}

  bool Wait() {
    // futex() can wake up spuriously if this memory address was previously used
    // for a pthread mutex. So, also check the condition.
    while (true) {
      int res = syscall(SYS_futex, &futex_, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0,
                        nullptr, nullptr, 0);
      if (rtc::AtomicOps::AcquireLoad(&futex_) != 0)
        return true;
      if (res != 0)
        return false;
    }
  }

  void Signal() {
    rtc::AtomicOps::ReleaseStore(&futex_, 1);
    syscall(SYS_futex, &futex_, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr,
            nullptr, 0);
  }

 private:
  int futex_;
};

// Struct to store the arguments to the signal handler.
struct SignalHandlerParams {
  // This event is signalled when signal handler is done executing.
  AsyncSafeWaitableEvent signal_handler_finish_event;
  // Running counter of array index below.
  size_t stack_size_cnt = 0;
  // Array storing the stack trace.
  uintptr_t addresses[kMaxStackSize];
};

// Global parameter used by the signal handler.
static SignalHandlerParams* g_signal_handler_params;

// This function is called iteratively for each stack trace element and stores
// the element in the array from |trace_argument|.
_Unwind_Reason_Code UnwindBacktrace(struct _Unwind_Context* trace,
                                    void* trace_argument) {
  SignalHandlerParams* const params =
      static_cast<SignalHandlerParams*>(trace_argument);

  // Avoid overflowing the stack trace array.
  if (params->stack_size_cnt >= kMaxStackSize)
    return _URC_END_OF_STACK;

  // Store the instruction pointer in the array.
  params->addresses[params->stack_size_cnt] = _Unwind_GetIP(trace);
  ++params->stack_size_cnt;

  return _URC_NO_REASON;
}

// This signal handler is exectued on the interrupted thread.
void SignalHandler(int n, siginfo_t* siginfo, void* sigcontext) {
  _Unwind_Backtrace(&UnwindBacktrace, g_signal_handler_params);
  g_signal_handler_params->signal_handler_finish_event.Signal();
}

}  // namespace

std::vector<StackTraceElement> GetStackTrace(rtc::PlatformThreadId tid) {
  // Only a thread itself can unwind its stack, so we will interrupt the given
  // tid with a custom signal handler in order to unwind its stack. The stack
  // will be recorded to |params| through the use of the global pointer
  // |g_signal_handler_param|.
  SignalHandlerParams params;
  g_signal_handler_params = &params;

  // Temporarily change the signal handler for our process to a function that
  // records a raw stack trace. The action we will change is for the signal
  // SIGURG ("urgent" or out-of-band data), because Android does not set up a
  // specific handler for this signal.

  // Change signal action for signal SIGURG and record the old handler.
  struct sigaction act;
  struct sigaction old_act;
  memset(&act, 0, sizeof(act));
  act.sa_sigaction = &SignalHandler;
  act.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&act.sa_mask);
  if (sigaction(SIGURG, &act, &old_act) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to change signal action with error " << errno;
    return std::vector<StackTraceElement>();
  }

  // Interrupt the thread with signal SIGURG. This will execute SignalHandler()
  // on the given thread.
  if (tgkill(getpid(), tid, SIGURG) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to interrupt thread " << tid << " with error "
                      << errno;
    return std::vector<StackTraceElement>();
  }

  // Wait until the thread is done recording its stack trace.
  if (!params.signal_handler_finish_event.Wait()) {
    RTC_LOG(LS_ERROR) << "Failed to wait for thread to finish stack trace "
                      << tid;
    return std::vector<StackTraceElement>();
  }

  // Restore previous signal handler.
  sigaction(SIGURG, &old_act, /* old_act= */ nullptr);

  if (params.stack_size_cnt >= kMaxStackSize)
    RTC_LOG(LS_WARNING) << "Stack trace for thread " << tid << " was truncated";

  // Translate program addresses into symbolic information using dladdr().
  std::vector<StackTraceElement> stack_trace;
  for (size_t i = 0; i < params.stack_size_cnt; ++i) {
    const uintptr_t address = params.addresses[i];

    Dl_info dl_info = {};
    if (!dladdr(reinterpret_cast<void*>(address), &dl_info)) {
      RTC_LOG(LS_WARNING)
          << "Could not translate address to symbolic information for address "
          << address << " at stack depth " << i;
      continue;
    }

    StackTraceElement stack_trace_element;
    stack_trace_element.shared_object_path = dl_info.dli_fname;
    stack_trace_element.program_counter =
        address - reinterpret_cast<uintptr_t>(dl_info.dli_fbase);
    stack_trace_element.symbol_name = dl_info.dli_sname;

    stack_trace.push_back(stack_trace_element);
  }

  return stack_trace;
}

std::string StackTraceToString(
    const std::vector<StackTraceElement>& stack_trace) {
  rtc::StringBuilder string_builder;

  for (size_t i = 0; i < stack_trace.size(); ++i) {
    const StackTraceElement& stack_trace_element = stack_trace[i];
    string_builder.AppendFormat("#%02d pc %08x %s", i,
                                stack_trace_element.program_counter,
                                stack_trace_element.shared_object_path);
    // The symbol name is only available for unstripped .so files.
    if (stack_trace_element.symbol_name != nullptr)
      string_builder.AppendFormat(" %s", stack_trace_element.symbol_name);

    string_builder.AppendFormat("\n");
  }

  return string_builder.str();
}

}  // namespace jni
}  // namespace webrtc
