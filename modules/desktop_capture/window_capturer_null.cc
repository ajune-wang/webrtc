/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

class WindowCapturerNull : public DesktopCapturer {
 public:
  WindowCapturerNull();
  ~WindowCapturerNull() override;

  WindowCapturerNull(const WindowCapturerNull&) = delete;
  WindowCapturerNull& operator=(const WindowCapturerNull&) = delete;

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  Callback* callback_ = nullptr;
};

WindowCapturerNull::WindowCapturerNull() {
  RTC_LOG(LS_INFO) << __func__;
}
WindowCapturerNull::~WindowCapturerNull() {
  RTC_LOG(LS_INFO) << __func__;
}

bool WindowCapturerNull::GetSourceList(SourceList* sources) {
  RTC_LOG(LS_INFO) << __func__;

  // Not implemented yet.
  return false;
}

bool WindowCapturerNull::SelectSource(SourceId id) {
  RTC_LOG(LS_INFO) << __func__;

  // Not implemented yet.
  return false;
}

void WindowCapturerNull::Start(Callback* callback) {
  RTC_LOG(LS_INFO) << __func__;

  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void WindowCapturerNull::CaptureFrame() {
  RTC_LOG(LS_INFO) << __func__;

  // Not implemented yet.
  callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
}

}  // namespace

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  RTC_LOG(LS_INFO) << __func__;

  return std::unique_ptr<DesktopCapturer>(new WindowCapturerNull());
}

}  // namespace webrtc
