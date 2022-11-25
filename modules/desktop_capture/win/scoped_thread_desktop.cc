/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/scoped_thread_desktop.h"

#include "modules/desktop_capture/win/desktop.h"
#include "rtc_base/logging.h"

namespace webrtc {

ScopedThreadDesktop::ScopedThreadDesktop()
    : initial_(Desktop::GetThreadDesktop()) {
  RTC_LOG(LS_INFO) << __func__;
}

ScopedThreadDesktop::~ScopedThreadDesktop() {
  RTC_LOG(LS_INFO) << __func__;

  Revert();
}

bool ScopedThreadDesktop::IsSame(const Desktop& desktop) {
  RTC_LOG(LS_INFO) << __func__;

  if (assigned_.get() != NULL) {
    return assigned_->IsSame(desktop);
  } else {
    return initial_->IsSame(desktop);
  }
}

void ScopedThreadDesktop::Revert() {
  RTC_LOG(LS_INFO) << __func__;

  if (assigned_.get() != NULL) {
    initial_->SetThreadDesktop();
    assigned_.reset();
  }
}

bool ScopedThreadDesktop::SetThreadDesktop(Desktop* desktop) {
  RTC_LOG(LS_INFO) << __func__;

  Revert();

  std::unique_ptr<Desktop> scoped_desktop(desktop);

  if (initial_->IsSame(*desktop))
    return true;

  if (!desktop->SetThreadDesktop())
    return false;

  assigned_.reset(scoped_desktop.release());
  return true;
}

}  // namespace webrtc
