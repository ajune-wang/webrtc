/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TRACING_LEGACY_TRACER_H_
#define RTC_BASE_TRACING_LEGACY_TRACER_H_

#include "absl/strings/string_view.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc::tracing {
RTC_EXPORT void SetupInternalTracer(bool enable_all_categories = true);
RTC_EXPORT bool StartInternalCapture(absl::string_view filename);
RTC_EXPORT void StartInternalCaptureToFile(FILE* file);
RTC_EXPORT void StopInternalCapture();
// Make sure we run this, this will tear down the internal tracing.
RTC_EXPORT void ShutdownInternalTracer();
}  // namespace webrtc::tracing

// TODO(webrtc:15917): Remove once upstreams fixed.
namespace rtc::tracing {
[[deprecated("Use the webrtc::tracing implementation")]] RTC_EXPORT inline void
SetupInternalTracer(bool enable_all_categories = true) {
  return webrtc::tracing::SetupInternalTracer(enable_all_categories);
}
[[deprecated("Use the webrtc::tracing implementation")]] RTC_EXPORT inline bool
StartInternalCapture(absl::string_view filename) {
  return webrtc::tracing::StartInternalCapture(filename);
}
[[deprecated("Use the webrtc::tracing implementation")]] RTC_EXPORT inline void
StartInternalCaptureToFile(FILE* file) {
  return webrtc::tracing::StartInternalCaptureToFile(file);
}
[[deprecated("Use the webrtc::tracing implementation")]] RTC_EXPORT inline void
StopInternalCapture() {
  return webrtc::tracing::StopInternalCapture();
}
// Make sure we run this, this will tear down the internal tracing.
[[deprecated("Use the webrtc::tracing implementation")]] RTC_EXPORT inline void
ShutdownInternalTracer() {
  return webrtc::tracing::ShutdownInternalTracer();
}
}  // namespace rtc::tracing

#endif  // RTC_BASE_TRACING_LEGACY_TRACER_H_
