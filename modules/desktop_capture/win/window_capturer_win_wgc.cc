/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/window_capturer_win_wgc.h"

#include <utility>

#include "rtc_base/logging.h"
#include "rtc_base/win/get_activation_factory.h"

namespace WGC = ABI::Windows::Graphics::Capture;
using Microsoft::WRL::ComPtr;

namespace webrtc {

WindowCapturerWinWgc::WindowCapturerWinWgc() = default;
WindowCapturerWinWgc::~WindowCapturerWinWgc() = default;

bool WindowCapturerWinWgc::GetSourceList(SourceList* sources) {
  return source_enumerator_.FindAllWindows(sources);
}

bool WindowCapturerWinWgc::SelectSource(DesktopCapturer::SourceId id) {
  capture_source_ = WgcCaptureSource::Create(id);
  return capture_source_->IsCapturable();
}

void WindowCapturerWinWgc::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;

  // Create a Direct3D11 device to share amongst the WgcCaptureSessions. Many
  // parameters are nullptr as the implemention uses defaults that work well for
  // us.
  HRESULT hr = D3D11CreateDevice(
      /*adapter=*/nullptr, D3D_DRIVER_TYPE_HARDWARE,
      /*software_rasterizer=*/nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      /*feature_levels=*/nullptr, /*feature_levels_size=*/0, D3D11_SDK_VERSION,
      &d3d11_device_, /*feature_level=*/nullptr, /*device_context=*/nullptr);
  if (hr == DXGI_ERROR_UNSUPPORTED) {
    // If a hardware device could not be created, use WARP which is a high speed
    // software device.
    hr = D3D11CreateDevice(
        /*adapter=*/nullptr, D3D_DRIVER_TYPE_WARP,
        /*software_rasterizer=*/nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        /*feature_levels=*/nullptr, /*feature_levels_size=*/0,
        D3D11_SDK_VERSION, &d3d11_device_, /*feature_level=*/nullptr,
        /*device_context=*/nullptr);
  }

  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "Failed to create D3D11Device: " << hr;
  }
}

void WindowCapturerWinWgc::CaptureFrame() {
  RTC_DCHECK(callback_);

  if (!capture_source_) {
    RTC_LOG(LS_ERROR) << "Source hasn't been selected";
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    return;
  }

  if (!d3d11_device_) {
    RTC_LOG(LS_ERROR) << "No D3D11D3evice, cannot capture.";
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    return;
  }

  HRESULT hr;
  WgcCaptureSession* capture_session = nullptr;
  auto iter = ongoing_captures_.find(capture_source_->GetId());
  if (iter == ongoing_captures_.end()) {
    ComPtr<WGC::IGraphicsCaptureItem> item;
    hr = capture_source_->GetCaptureItem(&item);
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to create a GraphicsCaptureItem: " << hr;
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
      return;
    }

    auto iter_success_pair = ongoing_captures_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(capture_source_->GetId()),
        std::forward_as_tuple(d3d11_device_, item));
    RTC_DCHECK(iter_success_pair.second);
    capture_session = &iter_success_pair.first->second;
  } else {
    capture_session = &iter->second;
  }

  if (!capture_session->IsCaptureStarted()) {
    hr = capture_session->StartCapture();
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to start capture: " << hr;
      ongoing_captures_.erase(capture_source_->GetId());
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
      return;
    }
  }

  std::unique_ptr<DesktopFrame> frame;
  hr = capture_session->GetFrame(&frame);
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "GetFrame failed: " << hr;
    ongoing_captures_.erase(capture_source_->GetId());
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    return;
  }

  if (!frame) {
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_TEMPORARY,
                               /*frame=*/nullptr);
    return;
  }

  callback_->OnCaptureResult(DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

// static
std::unique_ptr<DesktopCapturer> WindowCapturerWinWgc::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<DesktopCapturer>(new WindowCapturerWinWgc());
}

}  // namespace webrtc
