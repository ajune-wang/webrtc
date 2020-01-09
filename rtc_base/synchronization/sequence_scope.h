/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_SYNCHRONIZATION_SEQUENCE_SCOPE_H_
#define RTC_BASE_SYNCHRONIZATION_SEQUENCE_SCOPE_H_

namespace webrtc {
class SequenceScope {
 public:
  static void* Current();

  explicit SequenceScope(void* token);
  ~SequenceScope();

  SequenceScope(const SequenceScope&) = delete;
  SequenceScope& operator=(const SequenceScope&) = delete;
  void* const previous_;
};
}  // namespace webrtc

#endif  // RTC_BASE_SYNCHRONIZATION_SEQUENCE_SCOPE_H_
