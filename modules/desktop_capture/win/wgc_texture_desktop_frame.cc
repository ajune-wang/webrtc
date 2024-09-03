/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_texture_desktop_frame.h"

#include <comdef.h>
#include <d3d11_4.h>

#include <utility>

#include "api/sequence_checker.h"
#include "modules/desktop_capture/win/desktop_capture_utils.h"
#include "rtc_base/logging.h"

using Microsoft::WRL::ComPtr;

namespace webrtc {

class WgcFrameTexture : public FrameTexture {
 public:
  explicit WgcFrameTexture(Handle handle,
                           DesktopSize size,
                           ComPtr<ID3D11Device> d3d11_device,
                           WgcVideoProcessor* video_processor)
      : FrameTexture(handle),
        size_(size),
        d3d11_device_(d3d11_device),
        video_processor_(video_processor) {}

  ~WgcFrameTexture() override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    if (handle_ != kInvalidHandle) {
      CloseHandle(HANDLE(handle_));
    }
  }

  std::unique_ptr<DesktopFrame> CreateDesktopFrameFromTexture() override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    // Open shared resource from handle on source texture D3D11 device.
    ComPtr<ID3D11Texture2D> texture = GetTextureFromHandle((HANDLE)handle_);
    if (!texture) {
      return nullptr;
    }
    ComPtr<ID3D11DeviceContext> device_context;
    d3d11_device_->GetImmediateContext(&device_context);
    if (!mapped_texture_) {
      HRESULT hr = CreateMappedTexture(texture);
      if (FAILED(hr)) {
        return nullptr;
      }
      device_context->CopySubresourceRegion(mapped_texture_.Get(), 0, 0, 0, 0,
                                            texture.Get(), 0, nullptr);
    }
    // Map texture data.
    D3D11_MAPPED_SUBRESOURCE map_info;
    _com_error error = device_context->Map(
        mapped_texture_.Get(), /*subresource_index=*/0, D3D11_MAP_READ,
        /*D3D11_MAP_FLAG_DO_NOT_WAIT=*/0, &map_info);
    if (error.Error() != S_OK) {
      RTC_LOG(LS_ERROR) << "Failed to map texture: "
                        << desktop_capture::utils::ComErrorToString(error);
      return nullptr;
    }

    std::unique_ptr<DesktopFrame> buffer =
        std::make_unique<BasicDesktopFrame>(size_);
    uint8_t* src_data = static_cast<uint8_t*>(map_info.pData);
    uint8_t* dst_data = buffer->data();
    const int width_in_bytes =
        buffer->size().width() * DesktopFrame::kBytesPerPixel;
    RTC_DCHECK_GE(buffer->stride(), width_in_bytes);
    RTC_DCHECK_GE(map_info.RowPitch, width_in_bytes);
    for (int i = 0; i < size_.height(); i++) {
      memcpy(dst_data, src_data, width_in_bytes);
      dst_data += buffer->stride();
      src_data += map_info.RowPitch;
    }
    device_context->Unmap(mapped_texture_.Get(), 0);
    return buffer;
  }

  bool CopyToNewTexture(Handle new_texture_handle) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    ComPtr<ID3D11Texture2D> texture = GetTextureFromHandle((HANDLE)handle_);
    if (!texture) {
      RTC_LOG(LS_ERROR) << "Failed to get texture from handle.";
      return false;
    }
    ComPtr<ID3D11Texture2D> dst_texture =
        GetTextureFromHandle((HANDLE)new_texture_handle);
    if (!dst_texture) {
      RTC_LOG(LS_ERROR) << "Failed to get texture from new handle.";
      return false;
    }
    D3D11_TEXTURE2D_DESC dst_desc;
    dst_texture->GetDesc(&dst_desc);
    if ((int32_t)dst_desc.Width != size_.width() ||
        (int32_t)dst_desc.Height != size_.height()) {
      RTC_LOG(LS_ERROR) << "@Destination texture size is not equal to source."
                        << "Source: " << size_.width() << "x" << size_.height()
                        << " Destination: " << dst_desc.Width << "x"
                        << dst_desc.Height;
      return false;
    }
    if (dst_desc.Format != DXGI_FORMAT_NV12) {
      RTC_LOG(LS_ERROR) << "Destination texture format is not NV12.";
    }
    // Need VideoProcessorBlt.
    if (!video_processor_->ConvertBGRATextureToNV12(texture.Get(),
                                                    dst_texture.Get())) {
      RTC_LOG(LS_ERROR) << "Failed to convert texture to NV12.";
      return false;
    }
    return true;
  }

 private:
  HRESULT CreateMappedTexture(ComPtr<ID3D11Texture2D> src_texture) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);

    D3D11_TEXTURE2D_DESC src_desc;
    src_texture->GetDesc(&src_desc);
    D3D11_TEXTURE2D_DESC map_desc;
    map_desc.Width = src_desc.Width;
    map_desc.Height = src_desc.Height;
    map_desc.MipLevels = src_desc.MipLevels;
    map_desc.ArraySize = src_desc.ArraySize;
    map_desc.Format = src_desc.Format;
    map_desc.SampleDesc = src_desc.SampleDesc;
    map_desc.Usage = D3D11_USAGE_STAGING;
    map_desc.BindFlags = 0;
    map_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    map_desc.MiscFlags = 0;
    return d3d11_device_->CreateTexture2D(&map_desc, nullptr, &mapped_texture_);
  }

  ComPtr<ID3D11Texture2D> GetTextureFromHandle(HANDLE handle) {
    ComPtr<ID3D11Device1> device1;
    _com_error error = d3d11_device_.As(&device1);
    if (error.Error() != S_OK) {
      RTC_LOG(LS_ERROR) << "Failed to get ID3D11Device1: "
                        << desktop_capture::utils::ComErrorToString(error);
      return nullptr;
    }
    // Open shared resource from handle on source texture D3D11 device.
    ComPtr<ID3D11Texture2D> texture;
    error =
        device1->OpenSharedResource1(HANDLE(handle), IID_PPV_ARGS(&texture));
    if (error.Error() != S_OK) {
      RTC_LOG(LS_ERROR) << "Failed to open texture handle: "
                        << desktop_capture::utils::ComErrorToString(error);
      return nullptr;
    }
    return texture;
  }

  DesktopSize size_;
  ComPtr<ID3D11Device> d3d11_device_;
  WgcVideoProcessor* video_processor_;
  ComPtr<ID3D11Texture2D> mapped_texture_;
  SequenceChecker sequence_checker_;
};

bool WgcVideoProcessor::PrepareVideoProcessor() {
  _com_error error = d3d11_device_->QueryInterface(
      _uuidof(ID3D11VideoDevice),
      reinterpret_cast<void**>(video_device_.GetAddressOf()));
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to get video device: "
                      << desktop_capture::utils::ComErrorToString(error);
    return false;
  }
  ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device_->GetImmediateContext(&device_context);
  error = device_context->QueryInterface(
      _uuidof(ID3D11VideoContext),
      reinterpret_cast<void**>(video_context_.GetAddressOf()));
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to get video context: "
                      << desktop_capture::utils::ComErrorToString(error);
    return false;
  }

  D3D11_VIDEO_PROCESSOR_CONTENT_DESC vp_desc;
  vp_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  vp_desc.InputFrameRate.Numerator = 60;
  vp_desc.InputFrameRate.Denominator = 1;
  vp_desc.InputWidth = size_.width();
  vp_desc.InputHeight = size_.height();
  vp_desc.OutputFrameRate.Numerator = 60;
  vp_desc.OutputFrameRate.Denominator = 1;
  vp_desc.OutputWidth = size_.width();
  vp_desc.OutputHeight = size_.height();
  vp_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  error = video_device_->CreateVideoProcessorEnumerator(&vp_desc,
                                                        &processor_enumerator_);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to CreateVideoProcessorEnumerator: "
                      << desktop_capture::utils::ComErrorToString(error);
    return false;
  }
  error = video_device_->CreateVideoProcessor(processor_enumerator_.Get(), 0,
                                              &video_processor_);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to CreateVideoProcessor: "
                      << desktop_capture::utils::ComErrorToString(error);
    return false;
  }
  video_context_->VideoProcessorSetStreamAutoProcessingMode(
      video_processor_.Get(), 0, FALSE);

  return true;
}

bool WgcVideoProcessor::ConvertBGRATextureToNV12(
    ID3D11Texture2D* input_texture,
    ID3D11Texture2D* output_texture) {
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
      D3D11_VPOV_DIMENSION_TEXTURE2D};
  output_view_desc.Texture2D.MipSlice = 0;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> output_view;
  _com_error error = video_device_->CreateVideoProcessorOutputView(
      output_texture, processor_enumerator_.Get(), &output_view_desc,
      &output_view);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to create output view: "
                      << desktop_capture::utils::ComErrorToString(error);
    return false;
  }

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.ArraySlice = 0;
  input_view_desc.Texture2D.MipSlice = 0;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
  error = video_device_->CreateVideoProcessorInputView(
      input_texture, processor_enumerator_.Get(), &input_view_desc,
      &input_view);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to create input view: "
                      << desktop_capture::utils::ComErrorToString(error);
    return false;
  }

  D3D11_TEXTURE2D_DESC dst_desc;
  output_texture->GetDesc(&dst_desc);
  if ((int32_t)dst_desc.Width != size_.width() ||
      (int32_t)dst_desc.Height != size_.height()) {
    RTC_LOG(LS_ERROR)
        << "@Conv Destination texture size is not equal to source."
        << "Source: " << size_.width() << "x" << size_.height()
        << " Destination: " << dst_desc.Width << "x" << dst_desc.Height;
    return false;
  }

  D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
  streams.Enable = TRUE;
  streams.pInputSurface = input_view.Get();

  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  error = output_texture->QueryInterface(
      _uuidof(IDXGIKeyedMutex),
      reinterpret_cast<void**>(keyed_mutex.GetAddressOf()));
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to get KeyedMutex: "
                      << desktop_capture::utils::ComErrorToString(error);
    return false;
  }
  HRESULT hr = keyed_mutex->AcquireSync(0, INFINITE);
  if (FAILED(hr)) {
    return false;
  }
  error = video_context_->VideoProcessorBlt(video_processor_.Get(),
                                            output_view.Get(), 0, 1, &streams);
  hr = keyed_mutex->ReleaseSync(0);
  RTC_DCHECK(SUCCEEDED(hr));
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to VideoProcessorBlt: "
                      << desktop_capture::utils::ComErrorToString(error);
    return false;
  }
  return true;
}

WgcTextureDesktopFrame::WgcTextureDesktopFrame(
    DesktopSize size,
    int stride,
    uint8_t* data,
    std::unique_ptr<FrameTexture> frame_texture)
    : DesktopFrame(size, stride, data, nullptr),
      owned_frame_texture_(std::move(frame_texture)) {
  texture_ = owned_frame_texture_.get();
}

// static
std::unique_ptr<WgcTextureDesktopFrame> WgcTextureDesktopFrame::Create(
    DesktopSize size,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
    WgcVideoProcessor* video_processor) {
  ComPtr<ID3D11Device> texture_device;
  texture->GetDevice(&texture_device);

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  _com_error error = texture->QueryInterface(IID_PPV_ARGS(&dxgi_resource));
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to get DXGI resource: "
                      << desktop_capture::utils::ComErrorToString(error);
    return nullptr;
  }
  HANDLE texture_handle;
  error = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &texture_handle);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "Failed to create shared handle: "
                      << desktop_capture::utils::ComErrorToString(error);
    return nullptr;
  }

  std::unique_ptr<WgcFrameTexture> frame_texture =
      std::make_unique<WgcFrameTexture>(texture_handle, size, texture_device,
                                        video_processor);
  return std::unique_ptr<WgcTextureDesktopFrame>(new WgcTextureDesktopFrame(
      size, size.width() * DesktopFrame::kBytesPerPixel, nullptr,
      std::move(frame_texture)));
}

}  // namespace webrtc
