/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/base_capturer_pipewire.h"

#include "absl/types/optional.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

BaseCapturerPipeWire::BaseCapturerPipeWire(
    const DesktopCaptureOptions& options) {
  options_ = options;

  screencast_portal_ = std::make_unique<ScreenCastPortal>(
      ScreenCastPortal::CaptureSourceType::kAnyScreenContent);
}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {}

void BaseCapturerPipeWire::OnScreenCastRequestResult(
    ScreenCastPortal::RequestResponse result,
    uint32_t stream_node_id,
    uint32_t fd) {
  if (result == ScreenCastPortal::RequestResponse::SUCCESS) {
    if (!options_.screencast_stream()->StartScreenCastStream(stream_node_id,
                                                             fd)) {
      capturer_failed_ = true;
    }
    RTC_LOG(LS_INFO) << "ScreenCastPortal call successfully finished.";
  } else {
    capturer_failed_ = true;
    RTC_LOG(LS_ERROR) << "ScreenCastPortal failed.";
  }
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;

  screencast_portal_->Start(this);
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (capturer_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  absl::optional<std::unique_ptr<BasicDesktopFrame>> frame =
      options_.screencast_stream()->CaptureFrame();

  if (!frame) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on
  // the frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame.value()));
}

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  // List of available screens is already presented by the xdg-desktop-portal.
  // But we have to add an empty source as the code expects it.
  sources->push_back({0});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  // Screen selection is handled by the xdg-desktop-portal.
  return true;
}

// static
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.screencast_stream())
    return nullptr;

  return std::make_unique<BaseCapturerPipeWire>(options);
}

}  // namespace webrtc
