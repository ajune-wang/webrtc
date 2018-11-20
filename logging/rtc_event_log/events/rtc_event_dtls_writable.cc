/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_dtls_writable.h"

#include "absl/memory/memory.h"

namespace webrtc {

RtcEventDtlsWritable::RtcEventDtlsWritable(bool writable)
    : writable_(writable) {}

RtcEventDtlsWritable::RtcEventDtlsWritable(const RtcEventDtlsWritable& other)
    : writable_(other.writable_) {}

RtcEventDtlsWritable::~RtcEventDtlsWritable() = default;

RtcEvent::Type RtcEventDtlsWritable::GetType() const {
  return RtcEvent::Type::DtlsWritable;
}

bool RtcEventDtlsWritable::IsConfigEvent() const {
  return false;
}

std::unique_ptr<RtcEventDtlsWritable> RtcEventDtlsWritable::Copy() const {
  return absl::WrapUnique<RtcEventDtlsWritable>(
      new RtcEventDtlsWritable(*this));
}

}  // namespace webrtc
