/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SOURCE_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SOURCE_H_

#include <Windows.Graphics.Capture.h>
#include <wrl/client.h>
#include <memory>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/window_capture_utils.h"

namespace webrtc {

// WGC capturers use this class to represent the source that they are
// capturing from.
class WgcCaptureSource {
 public:
  virtual ~WgcCaptureSource();

  static std::unique_ptr<WgcCaptureSource> Create(
      DesktopCapturer::SourceId source_id);

  // IsCapturable indicates whether we can successfully capture from the
  // source, not if the captured frames will be useful or not. E.g. minimized
  // windows are capturable but empty frames will be returned.
  virtual bool IsCapturable() = 0;
  virtual HRESULT GetCaptureItem(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>* result) = 0;
  virtual DesktopCapturer::SourceId GetId() = 0;
};

class WgcWindowSource final : public WgcCaptureSource {
 public:
  explicit WgcWindowSource(DesktopCapturer::SourceId source_id);
  ~WgcWindowSource();

  bool IsCapturable() override;
  HRESULT GetCaptureItem(Microsoft::WRL::ComPtr<
                         ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>*
                             result) override;
  DesktopCapturer::SourceId GetId() override { return source_id_; }

 private:
  HRESULT CreateCaptureItem(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>* result);

  DesktopCapturer::SourceId source_id_;
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>
      item_;
};

// Use this class to find capturable sources.
class WgcWindowSourceEnumerator {
 public:
  WgcWindowSourceEnumerator();
  ~WgcWindowSourceEnumerator();

  bool FindAllWindows(DesktopCapturer::SourceList* sources);

 private:
  WindowCaptureHelperWin window_capture_helper_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SOURCE_H_
