/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/desktop_frame_texture.h"

namespace webrtc {

DesktopFrameTexture::DesktopFrameTexture(DesktopSize size)
  : DesktopFrame(size,
                 kBytesPerPixel,
                 nullptr,
                 nullptr) {
  is_texture_ = true;
}

DesktopFrameTexture::~DesktopFrameTexture() = default;


void DesktopFrameTexture::Init(const webrtc::D3dDevice& device,
                               HANDLE handle) {
  d3d_device_ = device.d3d_device();
  handle_ = handle;
}

std::unique_ptr<DesktopFrameTexture> DesktopFrameTexture::Share() {
  std::unique_ptr<DesktopFrameTexture> result(
      new DesktopFrameTexture(size()));
  result->d3d_device_ = d3d_device_;
  result->handle_ = handle_;
  return result;
}

}  // namespace webrtc
