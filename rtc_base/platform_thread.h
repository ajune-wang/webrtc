/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PLATFORM_THREAD_H_
#define RTC_BASE_PLATFORM_THREAD_H_

#include <functional>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/platform_thread_types.h"

namespace rtc {

enum class ThreadPriority {
  kLow = 1,
  kNormal,
  kHigh,
  kRealtime,
};

struct ThreadAttributes {
  ThreadPriority priority = ThreadPriority::kNormal;
  ThreadAttributes& SetPriority(ThreadPriority priority_param) {
    priority = priority_param;
    return *this;
  }
};

// Represents a simple worker thread.
class PlatformThread final {
 public:
  // Handle is the base platform thread handle.
#if defined(WEBRTC_WIN)
  using Handle = HANDLE;
  static constexpr Handle kNullHandle = nullptr;
#else
  using Handle = pthread_t;
  static constexpr Handle kNullHandle = 0;
#endif  // defined(WEBRTC_WIN)
  // This ctor creates the PlatformThread with a null handle (returning true in
  // empty) and is provided for convenience.
  PlatformThread() = default;

  // Moves |rhs| into this, storing an empty state in it.
  PlatformThread(PlatformThread&& rhs);

  // Moves |rhs| into this, storing an empty state in it.
  PlatformThread& operator=(PlatformThread&& rhs);

  // For a PlatformThread that's been spawned joinable, the destructor suspends
  // the calling thread until the created thread exits unless the thread has
  // already exited.
  virtual ~PlatformThread();

  // Finalizes any allocated resources.
  // For a PlatformThread that's been spawned joinable, Finalize() suspends
  // the calling thread until the created thread exits unless the thread has
  // already exited.
  // empty() returns true after completion.
  void Finalize();

  // Returns true if default constructed, moved from, or Finalize()ed.
  bool empty() const { return handle_ == kNullHandle; }

  // Creates a started joinable thread which will be joined when the returned
  // PlatformThread destructs or Finalize() is called.
  static PlatformThread SpawnJoinable(
      std::function<void()> thread_function,
      absl::string_view name,
      ThreadAttributes attributes = ThreadAttributes());

  // Creates a started detached thread. The caller has to use external
  // synchronization as nothing is provided by the PlatformThread construct.
  static PlatformThread SpawnDetached(
      std::function<void()> thread_function,
      absl::string_view name,
      ThreadAttributes attributes = ThreadAttributes());

  // Returns the base platform thread handle of this thread.
  Handle GetHandle() const;

#if defined(WEBRTC_WIN)
  // Queue a Windows APC function that runs when the thread is alertable.
  bool QueueAPC(PAPCFUNC apc_function, ULONG_PTR data);
#endif

 private:
  PlatformThread(Handle handle, bool joinable);
  static PlatformThread SpawnThread(std::function<void()> thread_function,
                                    absl::string_view name,
                                    ThreadAttributes attributes,
                                    bool joinable);

  Handle handle_ = kNullHandle;
  bool joinable_ = false;
};

}  // namespace rtc

#endif  // RTC_BASE_PLATFORM_THREAD_H_
