/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/event_tracer.h"

#include <stdio.h>

#include "rtc_base/trace_event.h"

#if defined(RTC_USE_PERFETTO)
#include "rtc_base/trace_categories.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#else
#endif

namespace webrtc {

namespace {

#if !defined(RTC_USE_PERFETTO)
GetCategoryEnabledPtr g_get_category_enabled_ptr = nullptr;
AddTraceEventPtr g_add_trace_event_ptr = nullptr;
#endif

}  // namespace

#if defined(RTC_USE_PERFETTO)
void RegisterPerfettoTrackEvents() {
  if (perfetto::Tracing::IsInitialized()) {
    webrtc::TrackEvent::Register();
  }
}
#else

void SetupEventTracer(GetCategoryEnabledPtr get_category_enabled_ptr,
                      AddTraceEventPtr add_trace_event_ptr) {
  g_get_category_enabled_ptr = get_category_enabled_ptr;
  g_add_trace_event_ptr = add_trace_event_ptr;
}

const unsigned char* EventTracer::GetCategoryEnabled(const char* name) {
  if (g_get_category_enabled_ptr)
    return g_get_category_enabled_ptr(name);

  // A string with null terminator means category is disabled.
  return reinterpret_cast<const unsigned char*>("\0");
}

// Arguments to this function (phase, etc.) are as defined in
// webrtc/rtc_base/trace_event.h.
void EventTracer::AddTraceEvent(char phase,
                                const unsigned char* category_enabled,
                                const char* name,
                                unsigned long long id,
                                int num_args,
                                const char** arg_names,
                                const unsigned char* arg_types,
                                const unsigned long long* arg_values,
                                unsigned char flags) {
  if (g_add_trace_event_ptr) {
    g_add_trace_event_ptr(phase, category_enabled, name, id, num_args,
                          arg_names, arg_types, arg_values, flags);
  }
}
#endif

}  // namespace webrtc
