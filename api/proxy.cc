/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/proxy.h"

namespace webrtc {
namespace internal {

void InvokeOnThread(const rtc::Location& posted_from,
                    rtc::Thread* t,
                    rtc::MessageHandler* handler) {
  // TODO(bugs.webrtc.org/11908): Remove thread policy workaround once calls in
  // chrome (nearby) don't need this.
#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
  bool invoke_was_allowed = true;
  rtc::Thread* current_thread = rtc::Thread::Current();
  if (current_thread) {
    invoke_was_allowed = current_thread->IsInvokeToThreadAllowed(t);
    if (!invoke_was_allowed)
      current_thread->AllowInvokesToThread(t);
  }
#endif

  t->Send(posted_from, handler);

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
  if (!invoke_was_allowed)
    current_thread->DisallowInvokesToThread(t);
#endif
}

}  // namespace internal
}  // namespace webrtc
