/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/sequence_scope.h"

#include "absl/base/attributes.h"
#include "absl/base/config.h"

namespace webrtc {
namespace {
#if defined(ABSL_HAVE_THREAD_LOCAL)

ABSL_CONST_INIT thread_local void* current_scope = nullptr;

void* GetCurrentScope() {
  return current_scope;
}

void SetCurrentScope(void* ptr) {
  current_scope = ptr;
}
#else
void* GetCurrentScope() {
  return nullptr;
}
void SetCurrentScope(void* ptr) {}
#endif
}  // namespace

void* SequenceScope::Current() {
  return GetCurrentScope();
}

SequenceScope::SequenceScope(void* token) : previous_(GetCurrentScope()) {
  SetCurrentScope(token);
}

SequenceScope::~SequenceScope() {
  SetCurrentScope(previous_);
}

}  // namespace webrtc
