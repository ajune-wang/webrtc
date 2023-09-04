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

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/win/d3d_device.h"
#include "modules/desktop_capture/shared_memory.h"
#include "rtc_base/system/no_unique_address.h"

namespace webrtc {

// ScopedHandle implementation used by captured frame of
// Windows IDXGIOutputDuplication API.
class TextureHandleWin : public ScopedHandle {
 public:
  TextureHandleWin(HANDLE handle, int id, int device_id);
  ~TextureHandleWin() override;
  SharedMemory::Handle ReleaseHandle() override;
  rtc::scoped_refptr<ScopedHandle> Duplicate() override;

  static rtc::scoped_refptr<TextureHandleWin>
  Create(HANDLE handle, int id, int device_id);
};

class TextureHandlePool {
 public:
  TextureHandlePool(int id, const D3dDevice& device);
  ~TextureHandlePool();

  static TextureHandlePool* GetInstance(int id);
  static int CreateInstance(const D3dDevice& device);
  static void DestroyInstance(int id);

  rtc::scoped_refptr<ScopedHandle> GetHandle(const DesktopSize& size);
  rtc::scoped_refptr<ScopedHandle> GetHandle(int handle_id);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTextureOfHandle(int handle_id);
  const D3dDevice& device() { return device_; }
  int id() { return id_; }

 private:

  static const int kPoolSize = 8;
  // TextureHandleWin accesses private FreeHandle() function.
  friend class TextureHandleWin;
  void OnHandleRelease(int handle_id);

  int id_;
//   Microsoft::WRL::ComPtr<ID3D11Device> device_;
  const D3dDevice device_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;

  std::map<int, HANDLE> handles_
      RTC_GUARDED_BY(sequence_checker_);
  std::map<int, Microsoft::WRL::ComPtr<ID3D11Texture2D>> textures_
      RTC_GUARDED_BY(sequence_checker_);
  std::map<int, int> handles_in_use_
      RTC_GUARDED_BY(sequence_checker_);
  std::map<int, DesktopSize> sizes_of_handles_
      RTC_GUARDED_BY(sequence_checker_);
  std::map<int, uint64_t> last_use_sequence_numbers_
      RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_DESKTOP_FRAME_TEXTURE_H_
