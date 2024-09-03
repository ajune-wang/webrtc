/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WGC_TEXTURE_DESKTOP_FRAME_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WGC_TEXTURE_DESKTOP_FRAME_H_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>

#include "modules/desktop_capture/desktop_frame.h"

namespace webrtc {

// Used to convert texture of WgcTextureDesktopFrameBGRA to NV12 destination.
class WgcVideoProcessor {
 public:
  WgcVideoProcessor(DesktopSize size,
                    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device)
      : size_(size), d3d11_device_(d3d11_device) {}
  virtual ~WgcVideoProcessor() = default;

  // Prepare video processor for NV12 texture converting.
  bool PrepareVideoProcessor();
  // Convert BGRA texture to shared NV12 texture.
  bool ConvertBGRATextureToNV12(ID3D11Texture2D* input_texture,
                                ID3D11Texture2D* output_texture);

 private:
  DesktopSize size_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  // Video processor variables.
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> processor_enumerator_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
};

// DesktopFrame implementation used by wgc captures on Windows.
// Frame texture is stored in the handle.
class WgcTextureDesktopFrame : public DesktopFrame {
 public:
  WgcTextureDesktopFrame(const WgcTextureDesktopFrame&) = delete;
  WgcTextureDesktopFrame& operator=(const WgcTextureDesktopFrame&) = delete;

  static std::unique_ptr<WgcTextureDesktopFrame> Create(
      DesktopSize size,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
      WgcVideoProcessor* video_processor);

 private:
  WgcTextureDesktopFrame(DesktopSize size,
                         int stride,
                         uint8_t* data,
                         std::unique_ptr<FrameTexture> frame_texture);

  std::unique_ptr<FrameTexture> owned_frame_texture_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WGC_TEXTURE_DESKTOP_FRAME_H_
