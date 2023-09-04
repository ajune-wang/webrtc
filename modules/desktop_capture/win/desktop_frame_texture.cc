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

#include <dxgi1_2.h>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

class PoolManager {
 public:
  PoolManager() = default;
  ~PoolManager() = default;

  static PoolManager* Instance() {
    static PoolManager* const pool_manager = new PoolManager();
    return pool_manager;
  }

  TextureHandlePool* Get(int id) {
    webrtc::MutexLock lock(&mutex_);
    if (pools_.count(id) > 0) {
      return pools_[id].get();
    }
    return nullptr;
  }

  int Create(const D3dDevice& device) {
    webrtc::MutexLock lock(&mutex_);
    next_pool_id_++;
    pools_.emplace(next_pool_id_,
        std::make_unique<TextureHandlePool>(next_pool_id_, device));
    return next_pool_id_;
  }

  void Destroy(int id) {
    webrtc::MutexLock lock(&mutex_);
    pools_.erase(id);
  }

 private:
  webrtc::Mutex mutex_;
  int next_pool_id_ RTC_GUARDED_BY(mutex_) = 0;
  std::map<int, std::unique_ptr<TextureHandlePool>> pools_
      RTC_GUARDED_BY(mutex_);
};

TextureHandlePool* TextureHandlePool::GetInstance(int id) {
  return PoolManager::Instance()->Get(id);
}

int TextureHandlePool::CreateInstance(
   const D3dDevice& device) {
  return PoolManager::Instance()->Create(device);
}

void TextureHandlePool::DestroyInstance(int id) {
  PoolManager::Instance()->Destroy(id);
}

TextureHandlePool::TextureHandlePool(int id, const D3dDevice& device)
    : id_(id), device_(device) {}

TextureHandlePool::~TextureHandlePool() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}

rtc::scoped_refptr<ScopedHandle>
TextureHandlePool::GetHandle(const DesktopSize& size) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (textures_.size() < kPoolSize) {
    // Create NV12 texture and shared handle.
    const DXGI_FORMAT dxgi_format = DXGI_FORMAT_NV12;
    D3D11_TEXTURE2D_DESC desc = {
      .Width = static_cast<UINT>(size.width()),
      .Height = static_cast<UINT>(size.height()),
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = dxgi_format,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = 0,
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                    D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device_.d3d_device()->CreateTexture2D(
        &desc, nullptr, texture.GetAddressOf());
    if (!SUCCEEDED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to CreateTexture2D";
      return nullptr;
    }
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    hr = texture.As(&dxgi_resource);
    if (!SUCCEEDED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to get IDXGIResource1.";
      return nullptr;
    }
    HANDLE handle;
    hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
      nullptr, &handle);
    if (!SUCCEEDED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to CreateSharedHandle.";
      return nullptr;
    }

    int handle_id = textures_.size();
    textures_.emplace(handle_id, texture);
    handles_in_use_.emplace(handle_id, 1);
    sizes_of_handles_.emplace(handle_id, size);
    handles_.emplace(handle_id, handle);
    last_use_sequence_numbers_.emplace(handle_id, 0);
    return TextureHandleWin::Create(handle, handle_id, id_);
  }

  int lru_handle = -1;
  for (auto& kv : sizes_of_handles_) {
    if (handles_in_use_[kv.first] == 0 && kv.second.equals(size)) {
      if (lru_handle < 0 || last_use_sequence_numbers_[kv.first] <
          last_use_sequence_numbers_[lru_handle]) {
        lru_handle = kv.first;
      }
    }
  }
  if (lru_handle >= 0) {
    handles_in_use_[lru_handle]++;
    return TextureHandleWin::Create(handles_[lru_handle], lru_handle, id_);
  } else {
    RTC_LOG(LS_ERROR) << "Maximum pool textures.";
    return nullptr;
  }
}

rtc::scoped_refptr<ScopedHandle>
TextureHandlePool::GetHandle(int handle_id) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (handles_in_use_.count(handle_id)) {
    handles_in_use_[handle_id]++;
    return TextureHandleWin::Create(handles_[handle_id], handle_id, id_);
  } else {
    RTC_LOG(LS_ERROR) << "Invalid handle_id for GetHandle.";
  }
  return nullptr;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D>
TextureHandlePool::GetTextureOfHandle(int handle_id) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (textures_.count(handle_id) > 0) {
    return textures_[handle_id];
  }
  return nullptr;
}

void TextureHandlePool::OnHandleRelease(int handle_id) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (handles_in_use_.count(handle_id) > 0) {
    handles_in_use_[handle_id]--;
    if (handles_in_use_[handle_id] == 0) {
      static uint64_t sequence_number = 0;
      last_use_sequence_numbers_[handle_id] = ++sequence_number;
    }
  }
}

TextureHandleWin::TextureHandleWin(HANDLE handle, int id, int device_id)
    : ScopedHandle(SharedMemory::Handle(handle), id, device_id) {}

TextureHandleWin::~TextureHandleWin() {
  if (IsValid()) {
    CloseHandle(HANDLE(handle_));
    TextureHandlePool* pool = TextureHandlePool::GetInstance(device_id_);
    if (pool) {
      pool->OnHandleRelease(id_);
    }
  }
}

SharedMemory::Handle TextureHandleWin::ReleaseHandle() {
  webrtc::SharedMemory::Handle released = handle_;
  handle_ = webrtc::SharedMemory::kInvalidHandle;
  if (released != webrtc::SharedMemory::kInvalidHandle) {
    TextureHandlePool* pool = TextureHandlePool::GetInstance(device_id_);
    if (pool) {
      pool->OnHandleRelease(id_);
    }
  }
  return released;
}

rtc::scoped_refptr<ScopedHandle> TextureHandleWin::Duplicate() {
  TextureHandlePool* pool = TextureHandlePool::GetInstance(device_id_);
  if (pool) {
    return pool->GetHandle(id_);
  }
  return nullptr;
}

rtc::scoped_refptr<TextureHandleWin>
TextureHandleWin::Create(HANDLE handle, int id, int device_id) {
  HANDLE new_handle;
  if (!DuplicateHandle(GetCurrentProcess(), handle,
                       GetCurrentProcess(), &new_handle, 0,
                       FALSE, DUPLICATE_SAME_ACCESS)) {
    RTC_LOG(LS_ERROR) << "Failed to duplicate handle.";
    return nullptr;
  }
  return rtc::scoped_refptr<TextureHandleWin>(
      new rtc::RefCountedObject<TextureHandleWin>(new_handle, id, device_id));
}


}  // namespace webrtc
