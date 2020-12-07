/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capture_source.h"

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directX.direct3d11.interop.h>
#include <utility>
#include <vector>

#include "rtc_base/win/get_activation_factory.h"

using Microsoft::WRL::ComPtr;
namespace WGC = ABI::Windows::Graphics::Capture;

namespace webrtc {

WgcCaptureSource::~WgcCaptureSource() = default;

// static
std::unique_ptr<WgcCaptureSource> WgcCaptureSource::Create(
    DesktopCapturer::SourceId source_id) {
  return std::make_unique<WgcWindowSource>(source_id);
}

WgcWindowSource::WgcWindowSource(DesktopCapturer::SourceId source_id)
    : source_id_(source_id) {}
WgcWindowSource::~WgcWindowSource() = default;

bool WgcWindowSource::IsCapturable() {
  return IsWindowValidAndVisible(reinterpret_cast<HWND>(source_id_)) &&
         (item_ || SUCCEEDED(CreateCaptureItem(&item_)));
}

HRESULT WgcWindowSource::GetCaptureItem(
    ComPtr<WGC::IGraphicsCaptureItem>* result) {
  HRESULT hr = S_OK;
  if (!item_)
    hr = CreateCaptureItem(&item_);

  *result = item_;
  return hr;
}

HRESULT WgcWindowSource::CreateCaptureItem(
    ComPtr<WGC::IGraphicsCaptureItem>* result) {
  if (!ResolveCoreWinRTDelayload())
    return E_FAIL;

  ComPtr<IGraphicsCaptureItemInterop> interop;
  HRESULT hr = GetActivationFactory<
      IGraphicsCaptureItemInterop,
      RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem>(&interop);
  if (FAILED(hr))
    return hr;

  ComPtr<WGC::IGraphicsCaptureItem> item;
  hr = interop->CreateForWindow(reinterpret_cast<HWND>(source_id_),
                                IID_PPV_ARGS(&item));
  if (FAILED(hr))
    return hr;

  if (!item)
    return E_HANDLE;

  *result = std::move(item);
  return hr;
}

WgcWindowSourceEnumerator::WgcWindowSourceEnumerator() = default;
WgcWindowSourceEnumerator::~WgcWindowSourceEnumerator() = default;

bool WgcWindowSourceEnumerator::FindAllWindows(
    DesktopCapturer::SourceList* results) {
  return window_capture_helper_.EnumerateCapturableWindows(results);
}

}  // namespace webrtc
