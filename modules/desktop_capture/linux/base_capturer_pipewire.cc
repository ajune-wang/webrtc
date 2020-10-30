/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/base_capturer_pipewire.h"

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/pipewire_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

BaseCapturerPipeWire::BaseCapturerPipeWire(
    const DesktopCaptureOptions& options,
    XdgDesktopPortalBase::CaptureSourceType type)
    : type_(type), id_(options.request_id()) {
  if (id_) {
    Init();
  }
}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {
  if (auto_close_connection_ ||
      XdgDesktopPortalBase::GetInstance().IsConnectionStreamingOnWeb(id_)) {
    XdgDesktopPortalBase::GetInstance().CloseConnection(id_);
  }
}

bool BaseCapturerPipeWire::Init() {
  init_called_ = true;

  XdgDesktopPortalBase::CaptureSourceType requestedType_ =
      XdgDesktopPortalBase::CaptureSourceType::kAny;

  // This will be in case where the browser doesn't specify ID and therefore
  // we will have one portal call for the preview dialog and another one for
  // the web page itself
  if (!id_) {
    id_ = g_random_int_range(0, G_MAXINT);
    auto_close_connection_ = true;
    requestedType_ = type_;
  }

  if (XdgDesktopPortalBase::GetInstance().IsConnectionInitialized(id_)) {
    // Because capturers created for the preview dialog (Chrome, Firefox) will
    // be created simultaneously and because of that the connection cannot be
    // initialized yet, we can safely assume this a capturer created in the
    // final state to show the content on the web page itself Note: this will
    // have no effect on clients not using our specific API in
    // DesktopCaptureOptions
    XdgDesktopPortalBase::GetInstance().SetConnectionStreamingOnWeb(id_);
    portal_initialized_ = true;
    return true;
  }

  auto lambda = [=](bool result) { portal_initialized_ = result; };

  rtc::Callback1<void, bool> cb = lambda;
  XdgDesktopPortalBase::GetInstance().InitPortal(cb, requestedType_, id_);

  return true;
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (!portal_initialized_) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  if (type_ != XdgDesktopPortalBase::CaptureSourceType::kAny &&
      XdgDesktopPortalBase::GetInstance()
              .GetConnectionData(id_)
              ->capture_source_type_ != type_ &&
      XdgDesktopPortalBase::GetInstance()
              .GetConnectionData(id_)
              ->capture_source_type_ !=
          XdgDesktopPortalBase::CaptureSourceType::kAny) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  const webrtc::PipeWireBase* pwBase =
      XdgDesktopPortalBase::GetInstance().GetPipeWireBase(id_);

  if (!pwBase) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  if (!pwBase->Frame()) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  const DesktopSize frame_size = pwBase->FrameSize();

  std::unique_ptr<DesktopFrame> result(new BasicDesktopFrame(frame_size));
  result->CopyPixelsFrom(
      pwBase->Frame(), (frame_size.width() * 4),  // kBytesPerPixel = 4
      DesktopRect::MakeWH(frame_size.width(), frame_size.height()));

  if (!result) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on the
  // frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);

  if (!init_called_) {
    Init();
  }

  sources->push_back({id_});

  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  if (!init_called_) {
    if (!id_) {
      id_ = id;
    }

    Init();
  }

  return true;
}

// static
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  std::unique_ptr<BaseCapturerPipeWire> capturer =
      std::make_unique<BaseCapturerPipeWire>(
          options, XdgDesktopPortalBase::CaptureSourceType::kScreen);

  return std::move(capturer);
}

// static
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  std::unique_ptr<BaseCapturerPipeWire> capturer =
      std::make_unique<BaseCapturerPipeWire>(
          options, XdgDesktopPortalBase::CaptureSourceType::kWindow);

  return std::move(capturer);
}

}  // namespace webrtc
