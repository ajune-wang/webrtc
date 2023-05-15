/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DESKTOP_FRAME_TEXTURE_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DESKTOP_FRAME_TEXTURE_H_

#include <d3d11.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/win/d3d_device.h"

namespace webrtc {

// DesktopFrame implementation used by capturers that use the
// Windows IDXGIOutputDuplication API.
class DesktopFrameTexture final : public DesktopFrame {
 public:
  explicit DesktopFrameTexture(DesktopSize size);
 
  DesktopFrameTexture(const DesktopFrameTexture&) = delete;
  DesktopFrameTexture& operator=(const DesktopFrameTexture&) = delete;

  ~DesktopFrameTexture() override;

  // Creates a clone of this object.
  std::unique_ptr<DesktopFrameTexture> Share();

 private:
  friend class DxgiOutputDuplicator;

  void Init(const webrtc::D3dDevice& device, HANDLE handle);
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_DESKTOP_FRAME_TEXTURE_H_
