/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_NATIVE_API_STACKTRACE_STACKTRACE_H_
#define SDK_ANDROID_NATIVE_API_STACKTRACE_STACKTRACE_H_

#include <string>
#include <vector>

namespace webrtc {

struct StackTraceElement {
  // Pathname of shared object (.so file) that contains address.
  const char* shared_object_path;
  // Program counter relative to .so base address.
  uint32_t program_counter;
  // Name of symbol whose definition overlaps the program counter. This value is
  // null when symbol names are stripped.
  const char* symbol_name;
};

// Utility to unwind stack for a given thread on Android ARM devices. This works
// on top of unwind.h and unwinds native (C++) stack traces only. This function
// does not provide any thread safety guarantees and the client must ensure
// synchronization between multiple calls to this function. This is because the
// implementation relies on overriding the process signal handler
std::vector<StackTraceElement> GetStackTrace(int tid);

// Get a string representation of the stack trace in a format ndk-stack accepts.
std::string StackTraceToString(
    const std::vector<StackTraceElement>& stack_trace);

}  // namespace webrtc

#endif  // SDK_ANDROID_NATIVE_API_STACKTRACE_STACKTRACE_H_
