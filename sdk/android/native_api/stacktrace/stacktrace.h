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
  // Program counter that is relative to the base address where the .so
  // file is stored.
  uint32_t program_counter;
  // Name of symbol whose definition overlaps the program counter. This value
  // might be null when no symbol names are available.
  const char* symbol_name;
};

// Utility to unwind stack for a given thread on Android ARM devices. This can
// can be useful for e.g. debugging deadlocks. This works on top of unwind.h and
// unwinds native (C++) stack traces only. This function does not provide any
// thread safety guarantees, and the client must ensure synchronization between
// multiple calls to this function. This is because the implementation relies on
// overriding the process signal handler. Note that this implementation is
// simple and does not contain all the bells and whistles of the official
// Android tools that e.g. generate tombstones. Also note that the
// implementation relies on low-level library calls that are potentially
// dangerous, so calling this function is a calculated risk and should
// preferably only be done when the app is in a bad state already.
std::vector<StackTraceElement> GetStackTrace(int tid);

// Get a string representation of the stack trace in the format ndk-stack
// expects.
std::string StackTraceToString(
    const std::vector<StackTraceElement>& stack_trace);

}  // namespace webrtc

#endif  // SDK_ANDROID_NATIVE_API_STACKTRACE_STACKTRACE_H_
